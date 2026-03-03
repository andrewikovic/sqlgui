#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>

namespace sqlgui::core {

template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        {
            std::scoped_lock lock(mutex_);
            queue_.push_back(std::move(item));
        }
        cv_.notify_one();
    }

    [[nodiscard]] std::optional<T> wait_pop(std::stop_token stop_token) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, stop_token, [this] { return !queue_.empty(); });
        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }

    [[nodiscard]] bool empty() const {
        std::scoped_lock lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<T> queue_;
};

}  // namespace sqlgui::core
