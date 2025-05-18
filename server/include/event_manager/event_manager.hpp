#pragma once
#include "thread_safe_queue/thread_safe_queue.hpp"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <thread>

namespace qabot::event_manager {
class EventManager {
  // singleton
public:
  static EventManager &getInstance() {
    static EventManager instance;
    return instance;
  }

  // 禁止複製和移動
  EventManager(const EventManager &) = delete;
  EventManager &operator=(const EventManager &) = delete;
  EventManager(EventManager &&) = delete;
  EventManager &operator=(EventManager &&) = delete;

  void addEvent(std::function<void()> event) {
    _eventQueue.push(event);
    _taskCondtion.notify_all();
  }

private:
  EventManager() {
    const int numThreads = std::thread::hardware_concurrency();

    for (int i = 0; i < numThreads; ++i) {
      _workers.emplace_back([this] { _workerLoop(); });
    }
  }
  ~EventManager() {
    _isRunning = false;
    _taskCondtion.notify_all();
    for (auto &worker : _workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    _workers.clear();
  }

  void _workerLoop() {
    while (_isRunning) {
      {
        std::unique_lock<std::mutex> lock(_taskMutex);
        if (_eventQueue.empty()) {
          _taskCondtion.wait(
              lock, [this] { return !_isRunning || !_eventQueue.empty(); });
        }

        if (!_isRunning) {
          break;
        }

        auto event = std::move(_eventQueue.tryPop());

        if (event.has_value()) {

          // call the eventss
          event.value()();
        }
      }
    }
  }

  thread_safe_queue::ThreadSafeQueue<std::function<void()>> _eventQueue;

  std::vector<std::thread> _workers;

  std::condition_variable _taskCondtion;

  mutable std::mutex _taskMutex;

  std::atomic_bool _isRunning{true};
};
} // namespace qabot::event_manager