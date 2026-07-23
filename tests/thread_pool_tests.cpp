#include "smart_home/thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
    smart_home::ThreadPool pool(4, 100);
    pool.start();
    std::atomic<int> completed(0);
    for (int i = 0; i < 50; ++i) {
        if (!pool.post([&completed] { ++completed; })) {
            std::cerr << "FAIL: accepted task was rejected" << std::endl;
            return 1;
        }
    }
    pool.stop();
    if (completed != 50 || pool.running()) {
        std::cerr << "FAIL: stop must drain queued tasks and join workers" << std::endl;
        return 1;
    }
    if (pool.post([] {})) {
        std::cerr << "FAIL: stopped pool accepted a task" << std::endl;
        return 1;
    }
    std::cout << "all thread pool tests passed" << std::endl;
    return 0;
}

