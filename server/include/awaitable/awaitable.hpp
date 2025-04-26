#pragma once

#include <atomic>
#include <coroutine>
#include <exception>
#include <functional>
#include <iostream>  // For logging
#include <optional>
#include <sstream>  // For logging thread ID
#include <thread>   // For std::this_thread::get_id

#include "thread_pool_singleton.hpp"

namespace qabot::awaitable {

// Common SharedState structure (assuming atomic_flag version)
template <typename T>
struct AwaitableSharedState {
  std::exception_ptr _exceptionPtr = nullptr;
  std::optional<T> _result;
  std::coroutine_handle<> handle = nullptr;
  std::atomic_flag completed_flag = ATOMIC_FLAG_INIT;
  // Add an ID for easier tracking
  size_t awaitable_id =
      std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
      std::chrono::high_resolution_clock::now().time_since_epoch().count();
};

// --- Awaitable<T> ---
template <typename T>
class Awaitable {
  using SharedState = AwaitableSharedState<T>;

 public:
  Awaitable(std::function<T()>&& func)
      : _func(std::move(func)), _sharedState(std::make_shared<SharedState>()) {
    // Log creation
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::cerr << "[" << ss.str() << "] Awaitable<T> ["
              << _sharedState->awaitable_id << "] Created." << std::endl;
  }

  bool await_ready() { return false; }

  void await_suspend(std::coroutine_handle<> handle) {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::cerr << "[" << ss.str() << "] Awaitable<T> ["
              << _sharedState->awaitable_id
              << "] await_suspend: Setting handle " << handle.address()
              << std::endl;

    if (!_func) {
      std::cerr << "[" << ss.str() << "] Awaitable<T> ["
                << _sharedState->awaitable_id << "] ERROR: Function not set!"
                << std::endl;
      throw std::runtime_error("Function not set");
    }
    _sharedState->handle = handle;
    auto state = _sharedState;
    auto func = std::move(_func);

    std::cerr << "[" << ss.str() << "] Awaitable<T> ["
              << _sharedState->awaitable_id
              << "] await_suspend: Enqueuing lambda." << std::endl;

    (void)thread_pool.enqueue([state, func]() {
      auto thread_id = std::this_thread::get_id();  // Get ID once
      std::cerr << "[" << thread_id << "] Awaitable<T> [" << state->awaitable_id
                << "] Lambda START. Handle: "
                << (state->handle ? state->handle.address() : nullptr)
                << std::endl;
      try {
        std::cerr << "[" << thread_id << "] Awaitable<T> ["
                  << state->awaitable_id << "] Lambda: Calling func()..."
                  << std::endl;
        state->_result.emplace(func());
        std::cerr << "[" << thread_id << "] Awaitable<T> ["
                  << state->awaitable_id
                  << "] Lambda: func() success. Result emplaced." << std::endl;
      } catch (...) {
        std::cerr << "[" << thread_id << "] Awaitable<T> ["
                  << state->awaitable_id << "] Lambda: func() EXCEPTION."
                  << std::endl;
        state->_exceptionPtr = std::current_exception();
      }

      std::cerr << "[" << thread_id << "] Awaitable<T> [" << state->awaitable_id
                << "] Lambda: Setting completed_flag." << std::endl;
      state->completed_flag.test_and_set(std::memory_order_release);
      state->completed_flag.notify_one();

      std::cerr << "[" << thread_id << "] Awaitable<T> [" << state->awaitable_id
                << "] Lambda: Checking handle before resume. Handle: "
                << (state->handle ? state->handle.address() : nullptr)
                << ", Done: " << (state->handle ? state->handle.done() : true)
                << std::endl;
      if (state->handle && !state->handle.done()) {
        std::cerr << "[" << thread_id << "] Awaitable<T> ["
                  << state->awaitable_id << "] Lambda: Calling resume()."
                  << std::endl;
        state->handle.resume();
        std::cerr << "[" << thread_id << "] Awaitable<T> ["
                  << state->awaitable_id << "] Lambda: resume() called."
                  << std::endl;
      } else {
        std::cerr << "[" << thread_id << "] Awaitable<T> ["
                  << state->awaitable_id
                  << "] Lambda: Handle invalid or done, NOT resuming."
                  << std::endl;
      }
      std::cerr << "[" << thread_id << "] Awaitable<T> [" << state->awaitable_id
                << "] Lambda END." << std::endl;
    });
    std::cerr << "[" << ss.str() << "] Awaitable<T> ["
              << _sharedState->awaitable_id
              << "] await_suspend: Returning void (coroutine suspended)."
              << std::endl;
  }

  T& await_resume() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::cerr << "[" << ss.str() << "] Awaitable<T> ["
              << _sharedState->awaitable_id << "] await_resume START. Handle: "
              << (_sharedState->handle ? _sharedState->handle.address()
                                       : nullptr)
              << std::endl;

    std::cerr << "[" << ss.str() << "] Awaitable<T> ["
              << _sharedState->awaitable_id
              << "] await_resume: Waiting for completed_flag..." << std::endl;
    _sharedState->completed_flag.wait(false, std::memory_order_acquire);
    std::cerr << "[" << ss.str() << "] Awaitable<T> ["
              << _sharedState->awaitable_id
              << "] await_resume: Flag acquired. Checking state..."
              << std::endl;

    if (_sharedState->_exceptionPtr) {
      std::cerr << "[" << ss.str() << "] Awaitable<T> ["
                << _sharedState->awaitable_id
                << "] await_resume: Rethrowing exception." << std::endl;
      std::rethrow_exception(_sharedState->_exceptionPtr);
    } else if (_sharedState->_result.has_value()) {
      std::cerr << "[" << ss.str() << "] Awaitable<T> ["
                << _sharedState->awaitable_id
                << "] await_resume: Returning result." << std::endl;
      return *_sharedState->_result;
    } else {
      std::cerr << "[" << ss.str() << "] Awaitable<T> ["
                << _sharedState->awaitable_id
                << "] await_resume: ERROR! Completed but no result/exception."
                << std::endl;
      throw std::runtime_error(
          "Awaitable completed but result missing (internal error)");
    }
  }

 private:
  std::function<T()> _func;
  std::shared_ptr<SharedState> _sharedState;
};

// --- Awaitable<void> ---
// (Apply similar detailed logging to Awaitable<void>)
template <>
class Awaitable<void> {
  using SharedState = AwaitableSharedState<char>;  // Use common state struct

 public:
  Awaitable(std::function<void()>&& func)
      : _func(std::move(func)), _sharedState(std::make_shared<SharedState>()) {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::cerr << "[" << ss.str() << "] Awaitable<void> ["
              << _sharedState->awaitable_id << "] Created." << std::endl;
  }

  bool await_ready() { return false; }

  void await_suspend(std::coroutine_handle<> handle) {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::cerr << "[" << ss.str() << "] Awaitable<void> ["
              << _sharedState->awaitable_id
              << "] await_suspend: Setting handle " << handle.address()
              << std::endl;

    if (!_func) {
      std::cerr << "[" << ss.str() << "] Awaitable<void> ["
                << _sharedState->awaitable_id << "] ERROR: Function not set!"
                << std::endl;
      throw std::runtime_error("Function not set");
    }
    _sharedState->handle = handle;
    auto state = _sharedState;
    auto func = std::move(_func);

    std::cerr << "[" << ss.str() << "] Awaitable<void> ["
              << _sharedState->awaitable_id
              << "] await_suspend: Enqueuing lambda." << std::endl;

    (void)thread_pool.enqueue([state, func]() {
      auto thread_id = std::this_thread::get_id();  // Get ID once
      std::cerr << "[" << thread_id << "] Awaitable<void> ["
                << state->awaitable_id << "] Lambda START. Handle: "
                << (state->handle ? state->handle.address() : nullptr)
                << std::endl;
      try {
        std::cerr << "[" << thread_id << "] Awaitable<void> ["
                  << state->awaitable_id << "] Lambda: Calling func()..."
                  << std::endl;
        func();
        std::cerr << "[" << thread_id << "] Awaitable<void> ["
                  << state->awaitable_id << "] Lambda: func() success."
                  << std::endl;
      } catch (...) {
        std::cerr << "[" << thread_id << "] Awaitable<void> ["
                  << state->awaitable_id << "] Lambda: func() EXCEPTION."
                  << std::endl;
        state->_exceptionPtr = std::current_exception();
      }
      // ... rest of void lambda ...
      std::cerr << "[" << thread_id << "] Awaitable<void> ["
                << state->awaitable_id << "] Lambda: Setting completed_flag."
                << std::endl;
      state->completed_flag.test_and_set(std::memory_order_release);
      state->completed_flag.notify_one();

      std::cerr << "[" << thread_id << "] Awaitable<void> ["
                << state->awaitable_id
                << "] Lambda: Checking handle before resume. Handle: "
                << (state->handle ? state->handle.address() : nullptr)
                << ", Done: " << (state->handle ? state->handle.done() : true)
                << std::endl;
      if (state->handle && !state->handle.done()) {
        std::cerr << "[" << thread_id << "] Awaitable<void> ["
                  << state->awaitable_id << "] Lambda: Calling resume()."
                  << std::endl;
        state->handle.resume();
        std::cerr << "[" << thread_id << "] Awaitable<void> ["
                  << state->awaitable_id << "] Lambda: resume() called."
                  << std::endl;
      } else {
        std::cerr << "[" << thread_id << "] Awaitable<void> ["
                  << state->awaitable_id
                  << "] Lambda: Handle invalid or done, NOT resuming."
                  << std::endl;
      }
      std::cerr << "[" << thread_id << "] Awaitable<void> ["
                << state->awaitable_id << "] Lambda END." << std::endl;
    });
    std::cerr << "[" << ss.str() << "] Awaitable<void> ["
              << _sharedState->awaitable_id
              << "] await_suspend: Returning void (coroutine suspended)."
              << std::endl;
  }

  void await_resume() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::cerr << "[" << ss.str() << "] Awaitable<void> ["
              << _sharedState->awaitable_id << "] await_resume START. Handle: "
              << (_sharedState->handle ? _sharedState->handle.address()
                                       : nullptr)
              << std::endl;

    std::cerr << "[" << ss.str() << "] Awaitable<void> ["
              << _sharedState->awaitable_id
              << "] await_resume: Waiting for completed_flag..." << std::endl;
    _sharedState->completed_flag.wait(false, std::memory_order_acquire);
    std::cerr << "[" << ss.str() << "] Awaitable<void> ["
              << _sharedState->awaitable_id
              << "] await_resume: Flag acquired. Checking state..."
              << std::endl;

    if (_sharedState->_exceptionPtr) {
      std::cerr << "[" << ss.str() << "] Awaitable<void> ["
                << _sharedState->awaitable_id
                << "] await_resume: Rethrowing exception." << std::endl;
      std::rethrow_exception(_sharedState->_exceptionPtr);
    }
    std::cerr << "[" << ss.str() << "] Awaitable<void> ["
              << _sharedState->awaitable_id << "] await_resume: Returning void."
              << std::endl;
  }

 private:
  std::function<void()> _func;
  std::shared_ptr<SharedState> _sharedState;
};

}  // namespace qabot::awaitable