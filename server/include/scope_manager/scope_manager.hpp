#pragma once

#include <list>

#include "task.hpp"

namespace qabot::scope_manager {
class ScopeManager {
 public:
  ScopeManager() = default;
  ~ScopeManager() = default;

  // 禁止複製和移動
  ScopeManager(const ScopeManager&) = delete;
  ScopeManager& operator=(const ScopeManager&) = delete;
  ScopeManager(ScopeManager&&) = delete;
  ScopeManager& operator=(ScopeManager&&) = delete;

  void addTask(qabot::task::Task<void> task) {
    // 將任務添加到任務列表中
    _tasks.emplace_back(std::move(task));
  }

  

 private:
  std::list<qabot::task::Task<void>> _tasks;
};
};  // namespace qabot::scope_manager