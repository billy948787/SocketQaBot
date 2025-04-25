#include <functional>
#include <list>

#include "thread_pool_singleton.hpp"

namespace qabot::executor {
class Executor {
 public:
  Executor& getInstance() {
    static Executor instance;
    return instance;
  }

  void submit(std::function<void()> func) { _tasks.push_back(func); }

  Executor(const Executor&) = delete;
  Executor& operator=(const Executor&) = delete;
  Executor(Executor&&) = delete;
  Executor& operator=(Executor&&) = delete;

 private:
  Executor() {
    thread_pool.enqueue([this]() {
      while (true) {
        for (auto& task : _tasks) {
          try {
            task();
            _tasks.remove(task);
            continue;
          } catch (const std::system_error& e) {
            // Handle exception
            if (e.code() != std::errc::operation_would_block &&
                e.code() != std::errc::resource_unavailable_try_again) {
              throw e;
            }
          } catch (const std::exception& e) {
            // Handle other exceptions
            throw e;
          }
        }
      }
    });
  };
  ~Executor() {
    for (auto& task : _tasks) {
      _tasks.clear();
    }
  }

  std::list<std::function<void()>> _tasks;
};
}  // namespace qabot::executor