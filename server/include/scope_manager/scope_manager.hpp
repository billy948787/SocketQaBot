#pragma once

#include <list>

#include "task/task.hpp"

namespace qabot::scope_manager {
class ScopeManager {
 public:
  // singleton
  static ScopeManager& getInstance() {
    static ScopeManager instance;
    return instance;
  }

  // 禁止複製和移動
  ScopeManager(const ScopeManager&) = delete;
  ScopeManager& operator=(const ScopeManager&) = delete;
  ScopeManager(ScopeManager&&) = delete;
  ScopeManager& operator=(ScopeManager&&) = delete;

  void addTask(qabot::task::Task<void>&& task) {
    // 將任務添加到任務列表中
    _tasks.emplace_back(std::move(task));
  }

  ScopeManager& operator<<(qabot::task::Task<void>&& task) {
    // 將任務添加到任務列表中
    _tasks.emplace_back(std::move(task));
    return *this;
  }

  void cleanUpTask() {
    _tasks.remove_if([](auto& task) { return task.isDone(); });
  }

 private:
  ScopeManager() = default;
  ~ScopeManager() {
    for (auto& task : _tasks) {
      _tasks.clear();
    }
  }
  std::list<qabot::task::Task<void>> _tasks;
};
};  // namespace qabot::scope_manager