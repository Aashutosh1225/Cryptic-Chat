#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

// MessageQueue<T>: thread-safe FIFO queue bridging two threads that must
// never touch shared state without synchronization -- specifically, the
// network-receive thread (ClientSession's receive loop, Phase 14 --
// pushes decrypted plaintext as it arrives) and the SFML render thread
// (ChatWindow::update(), Phase 11 -- pops them each frame to update the
// chat window).
//
// Header-only (template) -- no .cpp file, same as any other class template
// that needs to work for whatever T the caller instantiates it with.
//
// RECONCILED IN PHASE 14 against the real ui/ChatWindow.cpp (supplied
// this session): ChatWindow.cpp calls it as `concurrency::MessageQueue`
// and uses `while (std::optional<std::string> message = incoming_.tryPop())`
// -- i.e. it expects tryPop() to return std::optional<T>, in the
// `concurrency` namespace. The copy of this file in the original project
// files had neither: a bare (non-namespaced) `bool tryPop(T& out)`. This
// is exactly the divergence CONTINUE_HERE.md's Phase 11 section already
// flagged as a real possibility ("rebuilt from spec... slightly
// different API shape... re-verify against the true original if it ever
// resurfaces") -- it resurfaced, and now that ChatWindow's actual source
// is available, THIS version is normative: `concurrency::MessageQueue`
// with an `std::optional<T> tryPop()`.
//
// Every consumer (ClientSession.hpp, client_main.cpp, both client test
// files) spells the fully-qualified `concurrency::MessageQueue<T>` name
// explicitly -- an earlier draft of this file also added a global-scope
// `template <typename T> using MessageQueue = concurrency::MessageQueue<T>;`
// alias so unqualified code wouldn't need updating, but that tripped up
// IDE IntelliSense (a same-named alias template at global scope aliasing
// a nested template of the same name is a known confusion for some
// parsers -- it produced spurious "MessageQueue is not a template"
// errors even though it compiles fine with g++). Removed; explicit
// qualification is a few extra characters per use site and has no
// ambiguity for any tool to trip over.
namespace concurrency {

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
    // loop (ChatWindow::update()) uses each frame: check once, don't wait,
    // keep the UI responsive even when no message has arrived.
    // Returns the item if one was available, or std::nullopt otherwise --
    // callers drain with `while (auto item = queue.tryPop()) { ... }`.
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
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

} // namespace concurrency
