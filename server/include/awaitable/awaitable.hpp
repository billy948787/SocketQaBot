#pragma once
#include "event_manager/event_manager.hpp"

#include <coroutine>
#include <exception>
#include <functional>

#include "socket/socket.hpp"

namespace qabot::awaitable {
template <typename T> class Awaitable {
  struct SharedState {
    std::exception_ptr _exceptionPtr = nullptr;
    std::optional<T> _result;
    std::coroutine_handle<> handle = nullptr;
  };

public:
  Awaitable(std::function<T()> &&func)
      : _func(std::move(func)), _sharedState(std::make_shared<SharedState>()) {}

  bool await_ready() {
    // Check if the socket is ready for I/O operations
    try {
      _sharedState->_result.emplace(std::move(_func()));
      return true;
    } catch (const std::system_error &e) {
      if (e.code() == std::errc::operation_would_block ||
          e.code() == std::errc::resource_unavailable_try_again ||
          e.code() == std::errc::connection_already_in_progress
#ifdef _WIN32
          || e.code() == static_cast<std::errc>(WSAEWOULDBLOCK) ||
          e.code() == static_cast<std::errc>(WSAEALREADY)
#endif
      ) {
        // Handle non-blocking operation
        return false;
      } else {
        // Handle other exceptions
        _sharedState->_exceptionPtr = std::current_exception();
        return true;
      }
    } catch (const std::exception &e) {
      // Handle other exceptions
      _sharedState->_exceptionPtr = std::current_exception();
      return true;
    }
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

    _submitToEventLoop();
  }

  T &await_resume() {
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

  void _submitToEventLoop() {
    // Submit the function to the event loop
    event_manager::EventManager::getInstance().addEvent([this]() mutable {
      try {
        _sharedState->_result.emplace(std::move(_func()));

      } catch (const std::system_error &e) {
        // Handle exception
        if (e.code() == std::errc::operation_would_block ||
            e.code() == std::errc::resource_unavailable_try_again ||
            e.code() == std::errc::connection_already_in_progress
#ifdef _WIN32
            || e.code() == static_cast<std::errc>(WSAEWOULDBLOCK) ||
            e.code() == static_cast<std::errc>(WSAEALREADY)
#endif
        ) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          _submitToEventLoop();

          return;

        } else {
          _sharedState->_exceptionPtr = std::current_exception();
        }
      } catch (const std::exception &e) {
        // Handle other exceptions
        _sharedState->_exceptionPtr = std::current_exception();
      }

      if (_sharedState->handle) {
        _sharedState->handle.resume();
      }
    });
  }
};
template <> class Awaitable<void> {
  struct SharedState {
    std::exception_ptr _exceptionPtr = nullptr;
    std::coroutine_handle<> handle = nullptr;
  };

public:
  Awaitable(std::function<void()> &&func)
      : _func(std::move(func)), _sharedState(std::make_shared<SharedState>()) {}

  bool await_ready() {
    // Check if the socket is ready for I/O operations
    try {
      _func();
      return true;
    } catch (const std::system_error &e) {
      if (e.code() == std::errc::operation_would_block ||
          e.code() == std::errc::resource_unavailable_try_again ||
          e.code() == std::errc::connection_already_in_progress
#ifdef _WIN32
          || e.code() == static_cast<std::errc>(WSAEWOULDBLOCK) ||
          e.code() == static_cast<std::errc>(WSAEALREADY)
#endif
      ) {
        // Handle non-blocking operation
        return false;
      } else {
        // Handle other exceptions
        _sharedState->_exceptionPtr = std::current_exception();
        return true;
      }
    } catch (const std::exception &e) {
      // Handle other exceptions
      _sharedState->_exceptionPtr = std::current_exception();
      return true;
    }
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

    _submitToEventLoop();
  }

  void await_resume() {
    if (_sharedState->_exceptionPtr) {
      std::rethrow_exception(_sharedState->_exceptionPtr);
    }
  }

private:
  std::function<void()> _func;
  std::shared_ptr<SharedState> _sharedState;

  void _submitToEventLoop() {
    // Submit the function to the event loop
    event_manager::EventManager::getInstance().addEvent([this]() mutable {
      try {
        _func();
      } catch (const std::system_error &e) {
        // Handle exception
        if (e.code() == std::errc::operation_would_block ||
            e.code() == std::errc::resource_unavailable_try_again ||
            e.code() == std::errc::connection_already_in_progress ||
            e.code() == std::errc::operation_in_progress
#ifdef _WIN32
            || e.code() == static_cast<std::errc>(WSAEWOULDBLOCK) ||
            e.code() == static_cast<std::errc>(WSAEALREADY)
#endif
        ) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));

          _submitToEventLoop();

          return;

        } else {
          _sharedState->_exceptionPtr = std::current_exception();
        }
      } catch (const std::exception &e) {
        // Handle other exceptions
        _sharedState->_exceptionPtr = std::current_exception();
      }

      if (_sharedState->handle) {
        _sharedState->handle.resume();
      }
    });
  }
};
} // namespace qabot::awaitable