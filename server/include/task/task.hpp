#pragma once

#include <coroutine>
#include <optional>

namespace qabot::task {

template <typename T>
class Task {
 public:
  class promise_type;
  class promise_type {
   public:
    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() { return {}; }

    std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() {
      // TODO: Manage exceptions properly
    }  // Handle exceptions

    void return_void() {}  // Handle return value
  };
  T& result() {
    if (_result.has_value()) {
      return _result.value();
    } else {
      throw std::runtime_error("No result available");
    }
  }

  bool isDone() { return _handle.done() || !_handle; }

  // constructor
  Task(std::coroutine_handle<promise_type> h) : _handle(h) {}
  ~Task() { _handle.destroy(); }

 private:
  std::coroutine_handle<promise_type> _handle;

  std::optional<T> _result;
};

template <>
class Task<void> {
 public:
  class promise_type;
  class promise_type {
   public:
    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() { return {}; }

    std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() {}

    void return_void() {}  // Handle return value
  };

  bool isDone() { return _handle.done() || !_handle; }

  // constructor
  Task(std::coroutine_handle<promise_type> h) : _handle(h) {}
  ~Task() {
    if (_handle) {
      _handle.destroy();
    }
  }

  Task(Task&& other) noexcept : _handle(std::move(other._handle)) {
    other._handle = nullptr;
  }

 private:
  std::coroutine_handle<promise_type> _handle;
};
}  // namespace qabot::task