#include "smart_home/media_wire.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace smart_home {
namespace {
void put16(std::vector<std::uint8_t>& o, std::uint16_t v) { o.push_back(v >> 8); o.push_back(v & 255); }
void put32(std::vector<std::uint8_t>& o, std::uint32_t v) { o.push_back(v >> 24); o.push_back(v >> 16); o.push_back(v >> 8); o.push_back(v); }
void put64(std::vector<std::uint8_t>& o, std::uint64_t v) { for (int i=7;i>=0;--i) o.push_back(static_cast<std::uint8_t>(v >> (i*8))); }
std::uint16_t get16(const std::vector<std::uint8_t>& v, std::size_t& p) { if(p+2>v.size()) throw std::runtime_error("truncated media wire"); std::uint16_t x=(v[p]<<8)|v[p+1];p+=2;return x; }
std::uint32_t get32(const std::vector<std::uint8_t>& v, std::size_t& p) { if(p+4>v.size()) throw std::runtime_error("truncated media wire"); std::uint32_t x=(static_cast<std::uint32_t>(v[p])<<24)|(static_cast<std::uint32_t>(v[p+1])<<16)|(static_cast<std::uint32_t>(v[p+2])<<8)|v[p+3];p+=4;return x; }
std::uint64_t get64(const std::vector<std::uint8_t>& v, std::size_t& p) { if(p+8>v.size()) throw std::runtime_error("truncated media wire"); std::uint64_t x=0;for(int i=0;i<8;++i)x=(x<<8)|v[p++];return x; }
void require_end(const std::vector<std::uint8_t>& v, std::size_t p) { if(p!=v.size()) throw std::runtime_error("trailing media wire data"); }
}

std::vector<std::uint8_t> serialize_metadata(const VideoMetadata& v) {
    if(v.codec_extra.size()>std::numeric_limits<std::uint32_t>::max()) throw std::length_error("codec extra too large");
    std::vector<std::uint8_t> o; o.reserve(28+v.codec_extra.size());
    put32(o,v.camera_id);put32(o,v.codec_id);put32(o,v.width);put32(o,v.height);
    put32(o,static_cast<std::uint32_t>(v.time_base_num));put32(o,static_cast<std::uint32_t>(v.time_base_den));
    put32(o,static_cast<std::uint32_t>(v.codec_extra.size()));o.insert(o.end(),v.codec_extra.begin(),v.codec_extra.end());return o;
}
VideoMetadata deserialize_metadata(const std::vector<std::uint8_t>& w) {
    std::size_t p=0;VideoMetadata v;v.camera_id=get32(w,p);v.codec_id=get32(w,p);v.width=get32(w,p);v.height=get32(w,p);
    v.time_base_num=static_cast<std::int32_t>(get32(w,p));v.time_base_den=static_cast<std::int32_t>(get32(w,p));
    std::uint32_t n=get32(w,p);if(n>w.size()-p)throw std::runtime_error("invalid codec extra length");v.codec_extra.assign(w.begin()+p,w.begin()+p+n);p+=n;require_end(w,p);return v;
}
std::vector<std::uint8_t> serialize_video_packet(const VideoPacket& v) {
    if(v.data.size()>std::numeric_limits<std::uint32_t>::max())throw std::length_error("video packet too large");
    std::vector<std::uint8_t> o;o.reserve(32+v.data.size());put32(o,v.camera_id);put32(o,v.packet_id);put64(o,static_cast<std::uint64_t>(v.pts));put64(o,static_cast<std::uint64_t>(v.dts));put32(o,v.flags);put32(o,static_cast<std::uint32_t>(v.data.size()));o.insert(o.end(),v.data.begin(),v.data.end());return o;
}
VideoPacket deserialize_video_packet(const std::vector<std::uint8_t>& w) {
    std::size_t p=0;VideoPacket v;v.camera_id=get32(w,p);v.packet_id=get32(w,p);v.pts=static_cast<std::int64_t>(get64(w,p));v.dts=static_cast<std::int64_t>(get64(w,p));v.flags=get32(w,p);std::uint32_t n=get32(w,p);if(n>w.size()-p)throw std::runtime_error("invalid video data length");v.data.assign(w.begin()+p,w.begin()+p+n);p+=n;require_end(w,p);return v;
}
std::vector<std::uint8_t> serialize_fragment(const PacketFragment& f) { if(f.count==0||f.index>=f.count||f.data.size()>std::numeric_limits<std::uint32_t>::max())throw std::invalid_argument("invalid fragment");std::vector<std::uint8_t> o;put32(o,f.packet_id);put16(o,f.index);put16(o,f.count);put32(o,static_cast<std::uint32_t>(f.data.size()));o.insert(o.end(),f.data.begin(),f.data.end());return o; }
PacketFragment deserialize_fragment(const std::vector<std::uint8_t>& w) {std::size_t p=0;PacketFragment f;f.packet_id=get32(w,p);f.index=get16(w,p);f.count=get16(w,p);std::uint32_t n=get32(w,p);if(f.count==0||f.index>=f.count||n>w.size()-p)throw std::runtime_error("invalid fragment wire");f.data.assign(w.begin()+p,w.begin()+p+n);p+=n;require_end(w,p);return f;}
std::vector<PacketFragment> fragment_packet(std::uint32_t id,const std::vector<std::uint8_t>& w,std::size_t maximum) {if(maximum==0)throw std::invalid_argument("fragment size must be positive");std::size_t count=(w.size()+maximum-1)/maximum;if(count==0)count=1;if(count>65535)throw std::length_error("too many fragments");std::vector<PacketFragment> out;for(std::size_t i=0;i<count;++i){std::size_t b=i*maximum,e=std::min(w.size(),b+maximum);PacketFragment f={id,static_cast<std::uint16_t>(i),static_cast<std::uint16_t>(count),std::vector<std::uint8_t>(w.begin()+b,w.begin()+e)};out.push_back(f);}return out;}

PacketReassembler::PacketReassembler(std::size_t maximum):maximum_packet_size_(maximum){if(maximum==0)throw std::invalid_argument("maximum packet size must be positive");}
bool PacketReassembler::append(const PacketFragment& f,std::vector<std::uint8_t>& completed){if(f.count==0||f.index>=f.count)throw std::invalid_argument("invalid fragment");Assembly& a=assemblies_[f.packet_id];if(a.next_index==0){a.count=f.count;}if(a.count!=f.count||a.next_index!=f.index){assemblies_.erase(f.packet_id);throw std::runtime_error("out-of-order or inconsistent fragment");}if(a.data.size()+f.data.size()>maximum_packet_size_){assemblies_.erase(f.packet_id);throw std::length_error("reassembled packet exceeds maximum");}a.data.insert(a.data.end(),f.data.begin(),f.data.end());++a.next_index;if(a.next_index==a.count){completed.swap(a.data);assemblies_.erase(f.packet_id);return true;}return false;}
void PacketReassembler::discard(std::uint32_t id){assemblies_.erase(id);}std::size_t PacketReassembler::pending()const{return assemblies_.size();}
}  // namespace smart_home
