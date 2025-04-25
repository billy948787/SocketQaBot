#pragma once
#include <thread_pool/thread_pool.h>

#include <atomic>
#include <coroutine>
#include <exception>
#include <functional>

#include "socket.hpp"
#include "thread_pool_singleton.hpp"

namespace qabot::socket {
template <typename T>
class SocketAwaitable {
  struct SharedState {
    std::exception_ptr _exceptionPtr = nullptr;
    std::optional<T> _result;
    std::coroutine_handle<> handle = nullptr;
  };

 public:
  SocketAwaitable(std::function<T()>&& func)
      : _func(std::move(func)), _sharedState(std::make_shared<SharedState>()) {}

  bool await_ready() {
    // Check if the socket is ready for I/O operations
    return false;
  }

  void await_suspend(std::coroutine_handle<> handle) {
    // Suspend the coroutine and register the handle
    // This is where you would typically set up an event loop or I/O
    // multiplexing z
    if (!_func) {
      throw std::runtime_error("Function not set");
    }

    _sharedState->handle = handle;

    auto state = _sharedState;
    auto func = std::move(_func);

    (void)thread_pool.enqueue([state, func]() {
      while (true) {
        try {
          // Call the function and resume the coroutine
          state->_result.emplace(std::move(func()));

          // Resume the coroutine
          break;
        } catch (const std::system_error& e) {
          // Handle exception
          if (e.code() != std::errc::operation_would_block &&
              e.code() != std::errc::resource_unavailable_try_again) {
            state->_exceptionPtr = std::current_exception();
          }
        } catch (const std::exception& e) {
          // Handle other exceptions
          state->_exceptionPtr = std::current_exception();
        }
      }
      if (!state->handle.done()) {
        state->handle.resume();
      }
    });

    // Call the function with the provided arguments
  }

  T& await_resume() {
    if (_sharedState->_exceptionPtr) {
      std::rethrow_exception(_sharedState->_exceptionPtr);
    }
    if (_sharedState->_result.has_value()) {
      return *_sharedState->_result;
    } else {
      throw std::runtime_error("Function did not return a value");
    }
  }

 private:
  std::function<T()> _func;
  std::shared_ptr<SharedState> _sharedState;
};
template <>
class SocketAwaitable<void> {
  struct SharedState {
    std::exception_ptr _exceptionPtr = nullptr;
    std::coroutine_handle<> handle = nullptr;
  };

 public:
  SocketAwaitable(std::function<void()>&& func)
      : _func(std::move(func)), _sharedState(std::make_shared<SharedState>()) {}

  bool await_ready() {
    // Check if the socket is ready for I/O operations
    return false;
  }

  void await_suspend(std::coroutine_handle<> handle) {
    // Suspend the coroutine and register the handle
    // This is where you would typically set up an event loop or I/O
    // multiplexing z
    if (!_func) {
      throw std::runtime_error("Function not set");
    }

    _sharedState->handle = handle;
    auto state = _sharedState;
    auto func = std::move(_func);

    (void)thread_pool.enqueue([state, func]() {
      while (true) {
        try {
          // Call the function and resume the coroutine
          func();
          break;
        } catch (const std::system_error& e) {
          // Handle exception
          if (e.code() != std::errc::operation_would_block &&
              e.code() != std::errc::resource_unavailable_try_again) {
            state->_exceptionPtr = std::current_exception();
          }
        } catch (const std::exception& e) {
          // Handle other exceptions
          state->_exceptionPtr = std::current_exception();
        }
      }
      if (!state->handle.done()) {
        state->handle.resume();
      }
    });
  }

  void await_resume() {
    if (_sharedState->_exceptionPtr) {
      std::rethrow_exception(_sharedState->_exceptionPtr);
    }
  }

 private:
  std::function<void()> _func;
  std::shared_ptr<SharedState> _sharedState;
};
}  // namespace qabot::socket
