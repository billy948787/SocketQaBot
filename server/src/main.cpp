#include <iostream>

#include "event_manager/event_manager.hpp"
#include "scope_manager/scope_manager.hpp"
#include "server/server.hpp"

int main() {
  // Start the server
  qabot::server::Server::getInstance().start();

  // Keep the main thread alive
  while (true) {
    // Clean up completed tasks
    qabot::scope_manager::ScopeManager::getInstance().cleanUpTask();
  }

  return 0;
}
