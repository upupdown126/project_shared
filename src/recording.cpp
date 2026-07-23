#include "smart_home/recording.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#include <direct.h>
#define SMART_HOME_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define SMART_HOME_MKDIR(path) mkdir(path, 0755)
#endif

namespace smart_home {

InMemoryRecordingRepository::InMemoryRecordingRepository() : next_id_(1) {}
bool InMemoryRecordingRepository::insert(RecordingSegment& segment) {
    if (segment.end_ms <= segment.start_ms || segment.path.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    segment.id = next_id_++;
    segments_.push_back(segment);
    return true;
}
std::vector<RecordingSegment> InMemoryRecordingRepository::query(
    std::uint32_t camera, std::uint32_t channel, std::int64_t begin, std::int64_t end) {
    std::vector<RecordingSegment> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::size_t i=0;i<segments_.size();++i) {
        const RecordingSegment& s=segments_[i];
        if(s.camera_id==camera&&s.channel==channel&&s.end_ms>begin&&s.start_ms<end)result.push_back(s);
    }
    std::sort(result.begin(),result.end(),[](const RecordingSegment& a,const RecordingSegment& b){return a.start_ms<b.start_ms;});
    return result;
}
bool InMemoryRecordingRepository::find_by_id(std::uint64_t id, RecordingSegment& output) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        if (segments_[i].id == id) { output = segments_[i]; return true; }
    }
    return false;
}
std::vector<RecordingSegment> InMemoryRecordingRepository::list_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RecordingSegment> result = segments_;
    std::sort(result.begin(), result.end(), [](const RecordingSegment& a, const RecordingSegment& b) {
        return a.start_ms == b.start_ms ? a.id < b.id : a.start_ms < b.start_ms;
    });
    return result;
}
bool InMemoryRecordingRepository::erase(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::vector<RecordingSegment>::iterator it = segments_.begin(); it != segments_.end(); ++it) {
        if (it->id == id) { segments_.erase(it); return true; }
    }
    return false;
}

MountedStorage::MountedStorage(const std::string& root):root_(root){
    if(root_.empty())throw std::invalid_argument("storage root is empty");
    while(root_.size()>1&&(root_[root_.size()-1]=='/'||root_[root_.size()-1]=='\\'))root_.erase(root_.size()-1);
    create_directories(root_);
}
void MountedStorage::create_directories(const std::string& path){
    std::string current;
    for(std::size_t i=0;i<path.size();++i){char c=path[i];current.push_back(c);if((c=='/'||c=='\\')&&current.size()>1){SMART_HOME_MKDIR(current.c_str());}}
    SMART_HOME_MKDIR(current.c_str());
}
std::string MountedStorage::prepare_segment_path(std::uint32_t camera,std::uint32_t channel,std::int64_t start,std::uint32_t sequence)const{
    std::time_t seconds=static_cast<std::time_t>(start/1000);std::tm value;
#if defined(_WIN32)
    localtime_s(&value,&seconds);
#else
    localtime_r(&seconds,&value);
#endif
    std::ostringstream directory;directory<<root_<<"/camera_"<<camera<<"/channel_"<<channel<<'/'<<std::put_time(&value,"%Y%m%d");create_directories(directory.str());
    std::ostringstream file;file<<directory.str()<<'/'<<std::put_time(&value,"%Y%m%d_%H%M%S")<<'_'<<sequence<<".ts";return file.str();
}
bool MountedStorage::remove_file(const std::string& path)const{
    if(path.compare(0,root_.size(),root_)!=0 ||
       (path.size()>root_.size()&&path[root_.size()]!='/'&&path[root_.size()]!='\\'))return false;
    return std::remove(path.c_str())==0;
}
const std::string& MountedStorage::root()const{return root_;}

HlsPlaylistBuilder::HlsPlaylistBuilder(const std::string& prefix):media_url_prefix_(prefix){while(!media_url_prefix_.empty()&&media_url_prefix_[media_url_prefix_.size()-1]=='/')media_url_prefix_.erase(media_url_prefix_.size()-1);}
std::string HlsPlaylistBuilder::vod(const std::vector<RecordingSegment>& segments,const std::string& media_query)const{
    if(segments.empty())throw std::invalid_argument("cannot build an empty HLS playlist");
    double maximum=0;for(std::size_t i=0;i<segments.size();++i){if(segments[i].end_ms<=segments[i].start_ms)throw std::invalid_argument("invalid recording duration");maximum=std::max(maximum,(segments[i].end_ms-segments[i].start_ms)/1000.0);}
    std::ostringstream out;out<<"#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-PLAYLIST-TYPE:VOD\n#EXT-X-TARGETDURATION:"<<static_cast<long>(std::ceil(maximum))<<"\n#EXT-X-MEDIA-SEQUENCE:0\n";
    std::int64_t previous_end=segments[0].start_ms;
    for(std::size_t i=0;i<segments.size();++i){const RecordingSegment& s=segments[i];if(s.discontinuity||(i>0&&s.start_ms-previous_end>1000))out<<"#EXT-X-DISCONTINUITY\n";out<<"#EXTINF:"<<std::fixed<<std::setprecision(3)<<(s.end_ms-s.start_ms)/1000.0<<",\n"<<media_url_prefix_<<"/"<<s.id<<".ts"<<media_query<<"\n";previous_end=s.end_ms;}
    out<<"#EXT-X-ENDLIST\n";return out.str();
}
}  // namespace smart_home
