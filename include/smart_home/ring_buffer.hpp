#ifndef SMART_HOME_RING_BUFFER_HPP
#define SMART_HOME_RING_BUFFER_HPP

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace smart_home {

enum class RingOverflowPolicy { RejectNewest, DropOldest };

template <typename T>
class RingBuffer {
public:
    RingBuffer(std::size_t capacity, RingOverflowPolicy policy)
        : storage_(capacity + 1), head_(0), tail_(0), policy_(policy), closed_(false),
          dropped_(0) {
        if (capacity == 0) throw std::invalid_argument("ring buffer capacity must be positive");
    }

    bool push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) return false;
        std::size_t next = increment(tail_);
        if (next == head_) {
            if (policy_ == RingOverflowPolicy::RejectNewest) return false;
            head_ = increment(head_);
            ++dropped_;
        }
        storage_[tail_] = value;
        tail_ = next;
        available_.notify_one();
        return true;
    }

    bool pop(T& output) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (head_ == tail_) return false;
        output = storage_[head_];
        head_ = increment(head_);
        return true;
    }

    bool wait_pop(T& output, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!available_.wait_for(lock, timeout, [this] { return closed_ || head_ != tail_; }))
            return false;
        if (head_ == tail_) return false;
        output = storage_[head_];
        head_ = increment(head_);
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        available_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tail_ >= head_ ? tail_ - head_ : storage_.size() - head_ + tail_;
    }
    std::size_t capacity() const { return storage_.size() - 1; }
    std::size_t dropped() const { std::lock_guard<std::mutex> lock(mutex_); return dropped_; }
    bool closed() const { std::lock_guard<std::mutex> lock(mutex_); return closed_; }

private:
    std::size_t increment(std::size_t value) const { return (value + 1) % storage_.size(); }
    mutable std::mutex mutex_;
    std::condition_variable available_;
    std::vector<T> storage_;
    std::size_t head_;
    std::size_t tail_;
    RingOverflowPolicy policy_;
    bool closed_;
    std::size_t dropped_;
};

}  // namespace smart_home
#endif

