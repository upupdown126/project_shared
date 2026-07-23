#include "smart_home/ring_buffer.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main(){
 smart_home::RingBuffer<int> reject(2,smart_home::RingOverflowPolicy::RejectNewest);
 if(!reject.push(1)||!reject.push(2)||reject.push(3)||reject.size()!=2)return 1;
 int value=0;if(!reject.pop(value)||value!=1)return 1;
 smart_home::RingBuffer<int> drop(2,smart_home::RingOverflowPolicy::DropOldest);
 drop.push(1);drop.push(2);drop.push(3);if(drop.dropped()!=1||!drop.pop(value)||value!=2)return 1;
 smart_home::RingBuffer<int> waiting(1,smart_home::RingOverflowPolicy::RejectNewest);
 std::thread producer([&]{std::this_thread::sleep_for(std::chrono::milliseconds(20));waiting.push(9);});
 if(!waiting.wait_pop(value,std::chrono::milliseconds(500))||value!=9)return 1;producer.join();
 waiting.close();if(waiting.wait_pop(value,std::chrono::milliseconds(10))||waiting.push(4))return 1;
 std::cout<<"ring buffer tests passed"<<std::endl;return 0;
}
