#include "smart_home/recording_manager.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace smart_home {

struct RecordingManager::Job {
    explicit Job(std::unique_ptr<SegmentRecorder> value)
        : recorder(std::move(value)), running(true), stopping(false) {}
    std::unique_ptr<SegmentRecorder> recorder;
    std::thread worker;
    std::atomic<bool> running;
    std::atomic<bool> stopping;
    mutable std::mutex error_mutex;
    std::string error;
};

RecordingManager::RecordingManager(const SourceFactory& source_factory,
    const SegmentRecorder::WriterFactory& writer_factory,
    RecordingRepository& repository, const MountedStorage& storage,
    std::int64_t segment_duration_ms, std::int64_t retry_delay_ms)
    : source_factory_(source_factory), writer_factory_(writer_factory),
      repository_(repository), storage_(storage),
      segment_duration_ms_(segment_duration_ms), retry_delay_ms_(retry_delay_ms) {
    if (!source_factory_ || !writer_factory_ || segment_duration_ms_ <= 0 || retry_delay_ms_ < 0)
        throw std::invalid_argument("invalid recording manager configuration");
}

RecordingManager::~RecordingManager() { stop_all(); }

bool RecordingManager::start(std::uint32_t camera, std::uint32_t channel,
                             const std::string& url, std::string& message) {
    if (url.empty()) { message = "media URL is empty"; return false; }
    const Key key(camera, channel);
    std::shared_ptr<Job> job;
    try {
        std::unique_ptr<SegmentRecorder> recorder(new SegmentRecorder(
            source_factory_(), writer_factory_, repository_, storage_, segment_duration_ms_));
        job.reset(new Job(std::move(recorder)));
    } catch (const std::exception& error) {
        message = error.what();
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (jobs_.find(key) != jobs_.end()) {
            message = "recording job already exists; stop it before restarting";
            return false;
        }
        jobs_[key] = job;
    }
    const std::int64_t retry_delay = retry_delay_ms_;
    job->worker = std::thread([job, camera, channel, url, retry_delay] {
        bool reconnecting = false;
        while (!job->stopping) {
            try {
                if (reconnecting) job->recorder->mark_discontinuity();
                job->recorder->run(camera, channel, url);
                if (!job->stopping) {
                    std::lock_guard<std::mutex> lock(job->error_mutex);
                    job->error = "stream ended; reconnecting";
                }
            } catch (const std::exception& error) {
                std::lock_guard<std::mutex> lock(job->error_mutex);
                job->error = std::string("reconnecting after: ") + error.what();
            } catch (...) {
                std::lock_guard<std::mutex> lock(job->error_mutex);
                job->error = "reconnecting after unknown recording failure";
            }
            if (job->stopping) break;
            reconnecting = true;
            std::int64_t waited = 0;
            while (!job->stopping && waited < retry_delay) {
                const std::int64_t slice = std::min<std::int64_t>(100, retry_delay - waited);
                std::this_thread::sleep_for(std::chrono::milliseconds(slice)); waited += slice;
            }
        }
        job->running = false;
    });
    message = "recording started";
    return true;
}

bool RecordingManager::stop(std::uint32_t camera, std::uint32_t channel,
                            std::string& message) {
    std::shared_ptr<Job> job;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::map<Key, std::shared_ptr<Job> >::iterator found =
            jobs_.find(Key(camera, channel));
        if (found == jobs_.end()) { message = "recording job not found"; return false; }
        job = found->second;
        jobs_.erase(found);
    }
    job->stopping = true;
    job->recorder->request_stop();
    if (job->worker.joinable()) job->worker.join();
    message = "recording stopped";
    return true;
}

std::string RecordingManager::status(std::uint32_t camera, std::uint32_t channel) const {
    std::shared_ptr<Job> job;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::map<Key, std::shared_ptr<Job> >::const_iterator found =
            jobs_.find(Key(camera, channel));
        if (found == jobs_.end()) return "not-found";
        job = found->second;
    }
    std::lock_guard<std::mutex> lock(job->error_mutex);
    if (!job->error.empty()) return "failed " + job->error;
    return job->running ? "running" : "finished";
}

void RecordingManager::stop_all() {
    std::vector<std::shared_ptr<Job> > jobs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::map<Key, std::shared_ptr<Job> >::iterator it = jobs_.begin();
             it != jobs_.end(); ++it) jobs.push_back(it->second);
        jobs_.clear();
    }
    for (std::size_t i = 0; i < jobs.size(); ++i) {
        jobs[i]->stopping = true;
        jobs[i]->recorder->request_stop();
    }
    for (std::size_t i = 0; i < jobs.size(); ++i)
        if (jobs[i]->worker.joinable()) jobs[i]->worker.join();
}

RecordingProtocolHandler::RecordingProtocolHandler(RecordingManager& manager)
    : manager_(manager) {}

bool RecordingProtocolHandler::handle(const TcpServer::ConnectionPtr& connection,
                                      const TlvPacket& packet) {
    const TaskType type = static_cast<TaskType>(packet.type);
    if (type != TaskType::StartRecording && type != TaskType::StopRecording &&
        type != TaskType::RecordingStatus) return false;
    std::istringstream input(packet.text());
    std::uint32_t camera = 0, channel = 0;
    if (!(input >> camera >> channel)) {
        connection->send(TlvPacket(TaskType::Error,
            "recording payload: <camera_id> <channel> [rtsp_or_rtmp_url]"));
        return true;
    }
    if (type == TaskType::RecordingStatus) {
        connection->send(TlvPacket(TaskType::RecordingStatus,
            manager_.status(camera, channel)));
        return true;
    }
    std::string message;
    bool success = false;
    if (type == TaskType::StartRecording) {
        std::string url;
        if (!(input >> url)) {
            connection->send(TlvPacket(TaskType::Error, "recording URL is required"));
            return true;
        }
        success = manager_.start(camera, channel, url, message);
    } else {
        success = manager_.stop(camera, channel, message);
    }
    connection->send(TlvPacket(success ? TaskType::RecordingStatus : TaskType::Error,
                               message));
    return true;
}

}  // namespace smart_home
