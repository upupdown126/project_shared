#ifndef SMART_HOME_THREAD_POOL_HPP
#define SMART_HOME_THREAD_POOL_HPP

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace smart_home {

class ThreadPool {
public:
    ThreadPool(std::size_t worker_count, std::size_t maximum_tasks);
    ~ThreadPool();

    void start();
    bool post(const std::function<void()>& task);
    void stop();
    std::size_t pending() const;
    bool running() const;

private:
    ThreadPool(const ThreadPool&);
    ThreadPool& operator=(const ThreadPool&);
    void worker_loop();

    const std::size_t worker_count_;
    const std::size_t maximum_tasks_;
    mutable std::mutex mutex_;
    std::condition_variable task_available_;
    std::queue<std::function<void()> > tasks_;
    std::vector<std::thread> workers_;
    bool running_;
    bool stopping_;
};

}  // namespace smart_home

#endif

