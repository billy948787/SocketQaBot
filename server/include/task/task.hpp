#pragma once

#include <coroutine>
#include <exception>
#include <iostream>
#include <optional>

namespace qabot::task {

template <typename T>
class Task {
 public:
  class promise_type {
   public:
    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    class FinalAwaiter {
     public:
      bool await_ready() noexcept { return false; }

      void await_resume() noexcept {}

      // This is called when the coroutine is finished
      // and the final_suspend is called
      // if the coroutine is co_awaited by another coroutine
      // we need to resume the previous coroutine
      // otherwise we return a noop coroutine
      // to avoid the coroutine being destroyed
      // and the destructor being called
      // which will cause the program to crash
      std::coroutine_handle<> await_suspend(
          std::coroutine_handle<promise_type> h) {
        if (auto previousHandle = h.promise().previousHandle; previousHandle) {
          return previousHandle;
        } else {
          return std::noop_coroutine();
        }
      }
    };

    std::suspend_never initial_suspend() { return {}; }

    // This is called when the coroutine is finished
    FinalAwaiter final_suspend() noexcept { return {}; }

    void unhandled_exception() {
      exceptionPtr = std::current_exception();
    }  // Handle exceptions

    void return_value(T&& value) {
      result = std::move(value);
    }  // Handle return value

    T result;
    std::coroutine_handle<> previousHandle;
    std::exception_ptr exceptionPtr = nullptr;
  };  // promise_type

  class Awaitable {
   public:
    Awaitable(std::coroutine_handle<promise_type> h) : _handle(h) {}

    bool await_ready() noexcept { return false; }
    T& await_resume() noexcept {
      if (_handle.promise().exceptionPtr) {
        std::rethrow_exception(_handle.promise().exceptionPtr);
      }
      return _handle.promise().result;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
      // When the coroutine itself if co_await by other coroutine
      // we need to store the previous coroutine's handle
      // then resume the coroutine when the final_suspend is called
      _handle.promise().previousHandle = h;
      return _handle;
    }

   private:
    std::coroutine_handle<promise_type> _handle;
  };

  Awaitable operator co_await() {
    return Awaitable{_handle};
  }  // co_await operator

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
  class promise_type {
   public:
    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    class FinalAwaiter {
     public:
      bool await_ready() noexcept { return false; }

      void await_resume() noexcept {}

      std::coroutine_handle<> await_suspend(
          std::coroutine_handle<promise_type> h) {
        if (auto previousHandle = h.promise().previousHandle; previousHandle) {
          return previousHandle;
        } else {
          return std::noop_coroutine();
        }
      }
    };

    std::suspend_never initial_suspend() { return {}; }

    std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() { exceptionPtr = std::current_exception(); }

    void return_void() {}  // Handle return value

    std::coroutine_handle<> previousHandle;
    std::exception_ptr exceptionPtr = nullptr;
  };

  class Awaitable {
   public:
    Awaitable(std::coroutine_handle<promise_type> h) : _handle(h) {}

    bool await_ready() noexcept { return false; }
    void await_resume() noexcept {
      if (_handle.promise().exceptionPtr) {
        std::rethrow_exception(_handle.promise().exceptionPtr);
      }
    }

    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<promise_type> h) {
      _handle.promise().previousHandle = h;
      return _handle;
    }

   private:
    std::coroutine_handle<promise_type> _handle;
  };

  Awaitable operator co_await() {
    return Awaitable{_handle};
  }  // co_await operator

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