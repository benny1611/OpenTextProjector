#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    // Push an item into the queue
    void push(T value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push(std::move(value));
        _cond.notify_one();
    }

    // Wait until an item is available, then pop it
    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [this] { return !_queue.empty(); });
        value = std::move(_queue.front());
        _queue.pop();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }

private:
    std::queue<T> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _cond;
};