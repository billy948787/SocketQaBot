#pragma once
#include <thread_pool/thread_pool.h>

#include <coroutine>
#include <exception>
#include <functional>

#include "socket.hpp"
#include "thread_pool_singleton.hpp"

namespace qabot::socket {
template <typename T>
class SocketAwaitable {
 public:
  template <typename Class, typename... Args>
  SocketAwaitable(T (Class::*funcPtr)(Args...), Class* instance, Args&&... args)
      : _func(
            [funcPtr, instance,
             argsTuple = std::make_tuple(std::forward<Args>(args)...)]() -> T {
              return std::apply(
                  [&](auto&&... tupleArgs) -> T {
                    return std::invoke(funcPtr, instance, tupleArgs...);
                  },
                  std::move(argsTuple));
            }) {}

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

    (void)thread_pool.enqueue([&handle, this]() {
      while (true) {
        try {
          // Call the function and resume the coroutine
          _result.emplace(_func());
          // Resume the coroutine
          break;
        } catch (const std::exception& e) {
          // Handle exceptions
          _exceptionPtr = std::current_exception();
        }
      }
      handle.resume();
    });

    // Call the function with the provided arguments
  }

  T& await_resume() {
    if (_exceptionPtr) {
      std::rethrow_exception(_exceptionPtr);
    }
    if (_result.has_value()) {
      return *_result;
    } else {
      throw std::runtime_error("Function did not return a value");
    }
  }

 private:
  std::function<T()> _func;
  std::exception_ptr _exceptionPtr = nullptr;

  std::optional<T> _result;
};
template <>
class SocketAwaitable<void> {
 public:
  template <typename Class, typename... Args>
  SocketAwaitable(void (Class::*funcPtr)(Args...), Class* instance,
                  Args&&... args)
      : _func([funcPtr, instance,
               argsTuple = std::make_tuple(std::forward<Args>(args)...)]() {
          return std::apply(
              [&](auto&&... tupleArgs) {
                return std::invoke(funcPtr, instance, tupleArgs...);
              },
              std::move(argsTuple));
        }) {}

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

    (void)thread_pool.enqueue([&handle, this]() {
      while (true) {
        try {
          // Call the function and resume the coroutine
          _func();
        } catch (const std::exception& e) {
          // Handle exception
          _exceptionPtr = std::current_exception();
        }
      }
      handle.resume();
    });
  }

  void await_resume() {
    if (_exceptionPtr) {
      std::rethrow_exception(_exceptionPtr);
    }
  }

 private:
  std::function<void()> _func;
  std::exception_ptr _exceptionPtr = nullptr;
};
}  // namespace qabot::socket
