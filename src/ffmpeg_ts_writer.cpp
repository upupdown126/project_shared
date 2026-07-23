#include "smart_home/segment_recorder.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}
#include <cstring>
#include <stdexcept>
namespace smart_home {
namespace {std::string error_text(int c){char t[AV_ERROR_MAX_STRING_SIZE]={0};av_strerror(c,t,sizeof(t));return t;}}
struct FfmpegTsSegmentWriter::Implementation{AVFormatContext* format;AVStream* stream;AVRational input_time_base;Implementation():format(NULL),stream(NULL){input_time_base.num=0;input_time_base.den=1;}};
FfmpegTsSegmentWriter::FfmpegTsSegmentWriter():implementation_(new Implementation()){}
FfmpegTsSegmentWriter::~FfmpegTsSegmentWriter(){try{close();}catch(...){}}
void FfmpegTsSegmentWriter::open(const std::string& path,const VideoMetadata& m){close();int r=avformat_alloc_output_context2(&implementation_->format,NULL,"mpegts",path.c_str());if(r<0||!implementation_->format)throw std::runtime_error("create TS output failed: "+error_text(r));implementation_->stream=avformat_new_stream(implementation_->format,NULL);if(!implementation_->stream){close();throw std::runtime_error("create TS stream failed");}AVCodecParameters* p=implementation_->stream->codecpar;p->codec_type=AVMEDIA_TYPE_VIDEO;p->codec_id=static_cast<AVCodecID>(m.codec_id);p->width=m.width;p->height=m.height;if(!m.codec_extra.empty()){p->extradata=static_cast<std::uint8_t*>(av_mallocz(m.codec_extra.size()+AV_INPUT_BUFFER_PADDING_SIZE));if(!p->extradata){close();throw std::bad_alloc();}std::memcpy(p->extradata,&m.codec_extra[0],m.codec_extra.size());p->extradata_size=static_cast<int>(m.codec_extra.size());}implementation_->input_time_base=AVRational{m.time_base_num,m.time_base_den};implementation_->stream->time_base=implementation_->input_time_base;if(!(implementation_->format->oformat->flags&AVFMT_NOFILE)){r=avio_open(&implementation_->format->pb,path.c_str(),AVIO_FLAG_WRITE);if(r<0){close();throw std::runtime_error("open TS file failed: "+error_text(r));}}r=avformat_write_header(implementation_->format,NULL);if(r<0){close();throw std::runtime_error("write TS header failed: "+error_text(r));}}
void FfmpegTsSegmentWriter::write(const VideoPacket& value){if(!implementation_->format)throw std::runtime_error("TS writer is not open");AVPacket* p=av_packet_alloc();if(!p)throw std::bad_alloc();int r=av_new_packet(p,static_cast<int>(value.data.size()));if(r<0){av_packet_free(&p);throw std::runtime_error(error_text(r));}if(!value.data.empty())std::memcpy(p->data,&value.data[0],value.data.size());p->pts=value.pts;p->dts=value.dts;p->flags=value.flags;p->stream_index=implementation_->stream->index;av_packet_rescale_ts(p,implementation_->input_time_base,implementation_->stream->time_base);r=av_interleaved_write_frame(implementation_->format,p);av_packet_free(&p);if(r<0)throw std::runtime_error("write TS packet failed: "+error_text(r));}
void FfmpegTsSegmentWriter::close(){if(!implementation_->format)return;av_write_trailer(implementation_->format);if(!(implementation_->format->oformat->flags&AVFMT_NOFILE)&&implementation_->format->pb)avio_closep(&implementation_->format->pb);avformat_free_context(implementation_->format);implementation_->format=NULL;implementation_->stream=NULL;}
}  // namespace smart_home
