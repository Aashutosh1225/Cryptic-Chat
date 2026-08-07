#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace concurrency {

// Thread-safe FIFO queue used to bridge a dedicated network thread and the
// render/UI thread (or any producer/consumer thread pair).
//
// Two consumption modes are provided deliberately:
//   - tryPop(): non-blocking. Used by the render loop, which must never
//     stall waiting for network data -- it polls once per frame.
//   - waitAndPop(): blocking, wakes on push() or shutdown(). Used by a
//     dedicated thread (e.g. the network read loop) that has nothing
//     better to do while the queue is empty.
//
// shutdown() wakes every thread blocked in waitAndPop() so they can
// observe queue closure and exit cleanly instead of blocking forever.
template <typename T>
class MessageQueue {
public:
    MessageQueue() = default;

    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;

    void push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        condVar_.notify_one();
    }

    // Non-blocking. Returns std::nullopt immediately if the queue is empty.
    // Safe to call every frame from a render loop.
    std::optional<T> tryPop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
        {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // Blocks until an item is available or shutdown() is called.
    // Returns std::nullopt only when woken by shutdown() with an empty queue.
    std::optional<T> waitAndPop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condVar_.wait(lock, [this] { return !queue_.empty() || shuttingDown_; });

        if (queue_.empty())
        {
            // Woken by shutdown() with nothing left to drain.
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // Wakes all waiters; subsequent waitAndPop() calls return immediately
    // (draining any remaining items first, then std::nullopt once empty).
    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shuttingDown_ = true;
        }
        condVar_.notify_all();
    }

    bool isShutdown() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return shuttingDown_;
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condVar_;
    std::queue<T> queue_;
    bool shuttingDown_ = false;
};

} // namespace concurrency
