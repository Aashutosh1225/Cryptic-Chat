#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

// MessageQueue<T>: thread-safe FIFO queue bridging two threads that must
// never touch shared state without synchronization -- specifically, the
// network-receive thread (pushes decrypted Messages as they arrive) and
// the SFML render thread (pops them each frame to update the chat window).
//
// Header-only (template) -- no .cpp file, same as any other class template
// that needs to work for whatever T the caller instantiates it with.
template <typename T>
class MessageQueue {
public:
    // Adds an item to the back of the queue. Safe to call from any thread.
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();   // wake up one thread blocked in waitAndPop(), if any
    }

    // Non-blocking pop -- returns immediately. This is what the SFML render
    // loop uses each frame: check once, don't wait, keep the UI responsive
    // even when no message has arrived.
    // Returns true and fills `out` if an item was available, false otherwise.
    bool tryPop(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // Blocking pop -- waits until an item is available OR shutdown() is
    // called. Useful for a dedicated consumer thread that has nothing
    // else to do while waiting (unlike the render thread, which must keep
    // running its own loop and can't afford to block).
    // Returns true and fills `out` if an item was popped; returns false
    // only if shutdown() was called and the queue was empty at that point
    // (a clean "no more items are coming" signal, not an error).
    bool waitAndPop(T& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || shuttingDown_; });

        if (queue_.empty()) {
            return false;   // woke up because of shutdown(), not a new item
        }

        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // Wakes up any thread blocked in waitAndPop() so it can exit cleanly
    // (e.g. when the application is closing) instead of hanging forever
    // waiting for a message that will never come.
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shuttingDown_ = true;
        }
        cv_.notify_all();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool shuttingDown_ = false;
};