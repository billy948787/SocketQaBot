#include <chrono>
#include <coroutine>
#include <iostream>
#include <nlohmann/json.hpp>

#include "http/http_parse.hpp"
#include "http/http_serialize.hpp"
#include "scope_manager/scope_manager.hpp"
#include "socket/secure_socket.hpp"
#include "socket/socket.hpp"
#include "socket/socket_awaitable.hpp"
#include "task/task.hpp"
#ifdef _WIN32
#include "socket/windows_socket_impl.hpp"
using SocketImpl = qabot::socket::WindowsSocketImpl;
#else
#include "socket/unix_socket_impl.hpp"
using SocketImpl = qabot::socket::UnixSocketImpl;
#endif

qabot::task::Task<void> handleClient(
    qabot::socket::Socket<SocketImpl>&& clientSocket) {
  // 使用 std::shared_ptr 確保 clientSocket 在協程執行期間有效
  auto clientSocketPtr = std::make_shared<qabot::socket::Socket<SocketImpl>>(
      std::move(clientSocket));
  // 或者確保 Task 能正確處理傳入物件的生命週期
  try {
    while (true) {  // 循環接收來自此客戶端的訊息
      auto message = co_await qabot::socket::SocketAwaitable<std::string>(
          [clientSocketPtr]() -> std::string {
            return clientSocketPtr->receive(1024);
          });

      if (message.empty()) {
        // Client disconnected
        std::cout << "Client disconnected." << std::endl;
        break;
      }

      std::string response = qabot::http::serializeResponse(
          qabot::http::ResponseStatus::OK,
          {{"Content-Type",
            qabot::http::contentTypeToString(qabot::http::ContentType::JSON)}},
          nlohmann::json({{"你才是", "白癡"}}).dump());

      co_await qabot::socket::SocketAwaitable<void>(
          [clientSocketPtr, response]() { clientSocketPtr->send(response); });
    }
  } catch (const std::exception& e) {
    std::cerr << "Error handling client: " << e.what() << std::endl;
  }
}

qabot::task::Task<void> serverAcceptLoop(
    qabot::socket::Socket<SocketImpl>& serverSocket) {
  std::cout << "Start listening\n";

  while (true) {
    try {
      auto client = std::move(co_await qabot::socket::SocketAwaitable<
                              qabot::socket::Socket<SocketImpl>>(
          [&serverSocket]() -> qabot::socket::Socket<SocketImpl> {
            return serverSocket.accept();
          }));

      auto clientTask = handleClient(std::move(client));
      // move clientTask into scopeManager
      // so it won't be destructed when the function returns
      // or goes to next loop
      qabot::scope_manager::ScopeManager::getInstance()
          << std::move(clientTask);
    } catch (const std::exception& e) {
      std::cerr << "Error accepting connection: " << e.what() << std::endl;
      continue;
    }
  }
}

int main() {
  qabot::socket::Socket<SocketImpl> listeningSocket(
      qabot::socket::TransportProtocol::TCP, qabot::socket::IPVersion::IPv4);

  qabot::socket::SecureSocket<SocketImpl> secureSocket(
      qabot::socket::TransportProtocol::TCP, qabot::socket::IPVersion::IPv4);

  secureSocket.connect("https://generativelanguage.googleapis.com", 443);

  nlohmann::json jsonData = {
      {"contents", {{"parts", {{"text", "Explain how AI works"}}}}}};

  auto request = qabot::http::serializeRequest(
      qabot::http::RequestMethod::POST,
      "https://generativelanguage.googleapis.com/v1beta/models/"
      "gemini-2.0-flash:generateContent?key="
      {{"Content-Type",
        qabot::http::contentTypeToString(qabot::http::ContentType::JSON)

      }},
      jsonData.dump());

  secureSocket.send(request);

  auto response = secureSocket.receive(1024);
  std::cout << "Response: " << response << std::endl;

  // listeningSocket.bind("localhost", 38763);

  // listeningSocket.listen(5);
  // // Start the server accept loop
  // try {
  //   auto serverTask = serverAcceptLoop(listeningSocket);

  //   while (true) {
  //     std::this_thread::sleep_for(std::chrono::seconds(5));

  //     // Clean up completed tasks
  //     qabot::scope_manager::ScopeManager::getInstance().cleanUpTask();
  //   }
  // } catch (const std::exception& e) {
  //   std::cerr << "Error in server accept loop: " << e.what() << std::endl;
  // }
}
