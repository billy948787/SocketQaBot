#include <iostream>

#include "env_reader/env_reader.hpp"
#include "event_manager/event_manager.hpp"
#include "scope_manager/scope_manager.hpp"
#include "server/server.hpp"

int main() {
  // read env
  qabot::env_reader::EnvReader::getInstance().readEnv("../.env");

  // Start the server
  qabot::server::Server::getInstance().start();

  // Keep the main thread alive
  while (true) {
    // Clean up completed tasks
    std::this_thread::sleep_for(std::chrono::seconds(2));
    qabot::scope_manager::ScopeManager::getInstance().cleanUpTask();
  }

  return 0;
}
