#include "smart_home/media_stream.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <atomic>
#include <cstring>
#include <stdexcept>

namespace smart_home {
namespace {
std::string ffmpeg_error(int code) { char text[AV_ERROR_MAX_STRING_SIZE]={0}; av_strerror(code,text,sizeof(text)); return text; }
int interrupt_read(void* opaque) { return static_cast<std::atomic<bool>*>(opaque)->load() ? 1 : 0; }
}

struct FfmpegMediaSource::Implementation {
    AVFormatContext* format;
    int stream;
    std::uint32_t camera;
    std::uint32_t packet_id;
    std::atomic<bool> stopping;
    Implementation():format(NULL),stream(-1),camera(0),packet_id(0),stopping(false){}
};

FfmpegMediaSource::FfmpegMediaSource():implementation_(new Implementation()) { avformat_network_init(); }
FfmpegMediaSource::~FfmpegMediaSource(){close();}

VideoMetadata FfmpegMediaSource::open(std::uint32_t camera_id,const std::string& url){
    close();implementation_->stopping=false;implementation_->camera=camera_id;implementation_->packet_id=0;
    implementation_->format=avformat_alloc_context();if(!implementation_->format)throw std::runtime_error("avformat_alloc_context failed");
    implementation_->format->interrupt_callback.callback=interrupt_read;implementation_->format->interrupt_callback.opaque=&implementation_->stopping;
    AVDictionary* options=NULL;av_dict_set(&options,"stimeout","10000000",0);av_dict_set(&options,"rw_timeout","10000000",0);
    int result=avformat_open_input(&implementation_->format,url.c_str(),NULL,&options);av_dict_free(&options);
    if(result<0){close();throw std::runtime_error("open media stream failed: "+ffmpeg_error(result));}
    result=avformat_find_stream_info(implementation_->format,NULL);if(result<0){close();throw std::runtime_error("stream info failed: "+ffmpeg_error(result));}
    implementation_->stream=av_find_best_stream(implementation_->format,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    if(implementation_->stream<0){close();throw std::runtime_error("video stream not found");}
    AVStream* stream=implementation_->format->streams[implementation_->stream];AVCodecParameters* p=stream->codecpar;
    VideoMetadata m;m.camera_id=camera_id;m.codec_id=p->codec_id;m.width=p->width;m.height=p->height;m.time_base_num=stream->time_base.num;m.time_base_den=stream->time_base.den;
    if(p->extradata&&p->extradata_size>0)m.codec_extra.assign(p->extradata,p->extradata+p->extradata_size);return m;
}

bool FfmpegMediaSource::read(VideoPacket& output){
    if(!implementation_->format)return false;AVPacket* packet=av_packet_alloc();if(!packet)throw std::runtime_error("av_packet_alloc failed");
    int result=0;while((result=av_read_frame(implementation_->format,packet))>=0){if(packet->stream_index==implementation_->stream){output.camera_id=implementation_->camera;output.packet_id=++implementation_->packet_id;output.pts=packet->pts;output.dts=packet->dts;output.flags=packet->flags;output.data.assign(packet->data,packet->data+packet->size);av_packet_free(&packet);return true;}av_packet_unref(packet);}
    av_packet_free(&packet);if(result==AVERROR_EOF||implementation_->stopping)return false;throw std::runtime_error("read media packet failed: "+ffmpeg_error(result));
}
void FfmpegMediaSource::request_stop(){implementation_->stopping=true;}
void FfmpegMediaSource::close(){if(implementation_->format)avformat_close_input(&implementation_->format);implementation_->stream=-1;}
}  // namespace smart_home
