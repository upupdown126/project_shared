#include "smart_home/media_wire.hpp"
#include <iostream>

int main(){
 smart_home::VideoMetadata m={7,27,1920,1080,1,90000,std::vector<std::uint8_t>{1,2,3}};
 smart_home::VideoMetadata mr=smart_home::deserialize_metadata(smart_home::serialize_metadata(m));
 if(mr.camera_id!=7||mr.width!=1920||mr.codec_extra!=m.codec_extra)return 1;
 smart_home::VideoPacket p={7,42,-1,123,1,std::vector<std::uint8_t>(5000,0xab)};
 std::vector<std::uint8_t> wire=smart_home::serialize_video_packet(p);
 std::vector<smart_home::PacketFragment> fs=smart_home::fragment_packet(p.packet_id,wire,1400);
 if(fs.size()!=4)return 1;
 smart_home::PacketReassembler r;std::vector<std::uint8_t> done;
 for(std::size_t i=0;i<fs.size();++i){smart_home::PacketFragment decoded=smart_home::deserialize_fragment(smart_home::serialize_fragment(fs[i]));if(r.append(decoded,done)!=(i+1==fs.size()))return 1;}
 smart_home::VideoPacket pr=smart_home::deserialize_video_packet(done);
 if(pr.packet_id!=42||pr.pts!=-1||pr.data!=p.data||r.pending()!=0)return 1;
 bool rejected=false;try{smart_home::PacketReassembler bad;bad.append(fs[1],done);}catch(...){rejected=true;}if(!rejected)return 1;
 std::cout<<"media wire fragmentation tests passed"<<std::endl;return 0;
}
