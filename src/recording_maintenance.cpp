#include "smart_home/recording_maintenance.hpp"
#include "smart_home/logger.hpp"
#include "smart_home/md5.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace smart_home {
namespace {
bool load_file(const std::string& path, std::string& data) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) return false;
    data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}
}

RecordingMaintenance::RecordingMaintenance(RecordingRepository& repository,
    const MountedStorage& storage, std::uint32_t retention_days, std::uint64_t maximum_bytes)
    : repository_(repository), storage_(storage), retention_days_(retention_days),
      maximum_bytes_(maximum_bytes), stopping_(false) {}
RecordingMaintenance::~RecordingMaintenance() { stop(); }

MaintenanceReport RecordingMaintenance::run_once(std::int64_t now) {
    std::vector<RecordingSegment> segments = repository_.list_all();
    MaintenanceReport report = {0, 0, 0, 0};
    for (std::size_t i = 0; i < segments.size(); ++i) report.remaining_bytes += segments[i].size_bytes;
    const std::int64_t cutoff = retention_days_ == 0 ? INT64_MIN :
        now - static_cast<std::int64_t>(retention_days_) * 24LL * 60LL * 60LL * 1000LL;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const bool over_capacity = maximum_bytes_ != 0 && report.remaining_bytes > maximum_bytes_;
        const bool expired = segments[i].end_ms < cutoff;
        std::string data;
        const bool exists = load_file(segments[i].path, data);
        if (!exists) {
            ++report.corrupt;
            if (repository_.erase(segments[i].id)) {
                ++report.deleted;
                report.remaining_bytes -= std::min(report.remaining_bytes, segments[i].size_bytes);
            }
            continue;
        }
        if (expired || over_capacity) {
            if (storage_.remove_file(segments[i].path) && repository_.erase(segments[i].id)) {
                ++report.deleted;
                report.remaining_bytes -= std::min(report.remaining_bytes, segments[i].size_bytes);
            }
            continue;
        }
        ++report.verified;
        if (!segments[i].checksum.empty() && md5_hex(data) != segments[i].checksum) ++report.corrupt;
    }
    return report;
}

void RecordingMaintenance::start(std::uint32_t interval_seconds) {
    if (interval_seconds == 0 || worker_.joinable())
        throw std::invalid_argument("invalid maintenance interval or already running");
    stopping_ = false;
    worker_ = std::thread([this, interval_seconds] {
        while (!stopping_) {
            try {
                const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                const MaintenanceReport report = run_once(now);
                if (report.deleted || report.corrupt) Logger::instance().warning(
                    "recording maintenance: deleted=" + std::to_string(report.deleted) +
                    ", corrupt=" + std::to_string(report.corrupt));
            } catch (const std::exception& error) {
                Logger::instance().error(std::string("recording maintenance failed: ") + error.what());
            }
            std::uint32_t waited = 0;
            while (!stopping_ && waited < interval_seconds * 10U) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); ++waited;
            }
        }
    });
}
void RecordingMaintenance::stop() {
    stopping_ = true;
    if (worker_.joinable()) worker_.join();
}

}  // namespace smart_home
