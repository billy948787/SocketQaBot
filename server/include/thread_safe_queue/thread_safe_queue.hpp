#pragma once
#include <mutex>
#include <optional>
#include <queue>

namespace qabot::thread_safe_queue {
template <typename T> class ThreadSafeQueue {
public:
  ThreadSafeQueue() = default;

  void push(const T &value) {
    std::lock_guard<std::mutex> lock(_queueLock);
    _queue.push(value);
  }

  void pop() {
    std::lock_guard<std::mutex> lock(_queueLock);
    if (!_queue.empty()) {
      _queue.pop();
    }
  }

  std::optional<T> tryPop() {
    std::lock_guard<std::mutex> lock(_queueLock);
    if (!_queue.empty()) {
      auto value = std::move(_queue.front());
      _queue.pop();
      return value;
    }
    return std::nullopt;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(_queueLock);
    return _queue.empty();
  }

private:
  std::queue<T> _queue;
  mutable std::mutex _queueLock;
};
} // namespace qabot::thread_safe_queue