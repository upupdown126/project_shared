#include "smart_home/segment_recorder.hpp"
#include "smart_home/md5.hpp"

#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace smart_home {

SegmentRecorder::SegmentRecorder(std::unique_ptr<MediaSource> source,
    const WriterFactory& factory, RecordingRepository& repository,
    const MountedStorage& storage, std::int64_t duration)
    : source_(std::move(source)), writer_factory_(factory), repository_(repository),
      storage_(storage), segment_duration_ms_(duration), stopping_(false),
      next_discontinuity_(false) {
    if(!source_||!writer_factory_||duration<=0)throw std::invalid_argument("invalid segment recorder configuration");
}
SegmentRecorder::~SegmentRecorder(){request_stop();if(writer_)try{writer_->close();}catch(...){}}
std::int64_t SegmentRecorder::now_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::uint64_t SegmentRecorder::file_size(const std::string& path){std::ifstream in(path.c_str(),std::ios::binary|std::ios::ate);if(!in)return 0;return static_cast<std::uint64_t>(in.tellg());}
std::string SegmentRecorder::file_checksum(const std::string& path){std::ifstream in(path.c_str(),std::ios::binary);if(!in)return std::string();std::string data((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());return md5_hex(data);}
void SegmentRecorder::finish_segment(std::int64_t end){if(!writer_)return;writer_->close();writer_.reset();current_.end_ms=end>current_.start_ms?end:current_.start_ms+1;current_.size_bytes=file_size(current_.path);current_.checksum=file_checksum(current_.path);if(!repository_.insert(current_))throw std::runtime_error("could not index recording segment");}
void SegmentRecorder::run(std::uint32_t camera,std::uint32_t channel,const std::string& url,std::size_t maximum){
    stopping_=false;VideoMetadata metadata=source_->open(camera,url);std::int64_t first_pts=0,last_pts=0;bool have_pts=false;std::uint32_t sequence=0;std::size_t count=0;
    try{VideoPacket packet;while(!stopping_&&source_->read(packet)){
        const std::int64_t timestamp=packet.pts!=INT64_MIN?packet.pts:packet.dts;
        const std::int64_t packet_ms=metadata.time_base_den!=0&&timestamp!=INT64_MIN?timestamp*1000LL*metadata.time_base_num/metadata.time_base_den:0;
        if(!writer_){writer_=writer_factory_();current_=RecordingSegment{0,camera,channel,storage_.prepare_segment_path(camera,channel,now_ms(),sequence++),now_ms(),0,0,"",next_discontinuity_};next_discontinuity_=false;writer_->open(current_.path,metadata);first_pts=packet_ms;have_pts=timestamp!=INT64_MIN;}
        else if(have_pts&&(packet.flags&1U)&&packet_ms-first_pts>=segment_duration_ms_){finish_segment(current_.start_ms+(last_pts-first_pts));writer_=writer_factory_();current_=RecordingSegment{0,camera,channel,storage_.prepare_segment_path(camera,channel,now_ms(),sequence++),now_ms(),0,0,"",false};writer_->open(current_.path,metadata);first_pts=packet_ms;}
        writer_->write(packet);last_pts=packet_ms;if(maximum&&++count>=maximum)break;
    }finish_segment(current_.start_ms+(have_pts?last_pts-first_pts:1));source_->close();}
    catch(...){if(writer_){try{writer_->close();}catch(...){}writer_.reset();}source_->close();throw;}
}
void SegmentRecorder::request_stop(){stopping_=true;if(source_)source_->request_stop();}
void SegmentRecorder::mark_discontinuity(){next_discontinuity_=true;}
}  // namespace smart_home
