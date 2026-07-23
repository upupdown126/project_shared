#include "smart_home/thread_pool.hpp"

#include "smart_home/logger.hpp"

#include <exception>
#include <stdexcept>

namespace smart_home {

ThreadPool::ThreadPool(std::size_t worker_count, std::size_t maximum_tasks)
    : worker_count_(worker_count),
      maximum_tasks_(maximum_tasks),
      running_(false),
      stopping_(false) {
    if (worker_count_ == 0 || maximum_tasks_ == 0) {
        throw std::invalid_argument("thread pool sizes must be positive");
    }
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }
    stopping_ = false;
    running_ = true;
    for (std::size_t i = 0; i < worker_count_; ++i) {
        workers_.push_back(std::thread(&ThreadPool::worker_loop, this));
    }
}

bool ThreadPool::post(const std::function<void()>& task) {
    if (!task) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || stopping_ || tasks_.size() >= maximum_tasks_) {
            return false;
        }
        tasks_.push(task);
    }
    task_available_.notify_one();
    return true;
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        stopping_ = true;
    }
    task_available_.notify_all();
    for (std::size_t i = 0; i < workers_.size(); ++i) {
        if (workers_[i].joinable()) {
            workers_[i].join();
        }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    workers_.clear();
    running_ = false;
    stopping_ = false;
}

std::size_t ThreadPool::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

bool ThreadPool::running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            task_available_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) {
                return;
            }
            task = tasks_.front();
            tasks_.pop();
        }
        try {
            task();
        } catch (const std::exception& error) {
            Logger::instance().error(std::string("worker task failed: ") + error.what());
        } catch (...) {
            Logger::instance().error("worker task failed with an unknown exception");
        }
    }
}

}  // namespace smart_home

