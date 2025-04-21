#include <coroutine>
#include <iostream>
#include <nlohmann/json.hpp>

#include "http/http_parser.hpp"
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
    qabot::socket::Socket<SocketImpl> clientSocket) {
  // 使用 std::shared_ptr 確保 clientSocket 在協程執行期間有效
  // 或者確保 Task 能正確處理傳入物件的生命週期
  auto sharedClient = std::make_shared<qabot::socket::Socket<SocketImpl>>(
      std::move(clientSocket));
  try {
    while (true) {  // 循環接收來自此客戶端的訊息
      auto message = co_await qabot::socket::SocketAwaitable<std::string>(
          &qabot::socket::Socket<SocketImpl>::receive, sharedClient.get(),
          (size_t)1024);

      if (message.empty()) {
        // 對方可能關閉了連線
        std::cout << "Client disconnected." << std::endl;
        break;
      }

      std::cout << "Received: " << message << "\n";
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
          &qabot::socket::Socket<SocketImpl>::accept, &serverSocket));

      auto clientTask = handleClient(std::move(client));
    } catch (const std::exception& e) {
      std::cerr << "Error accepting connection: " << e.what() << std::endl;
      continue;
    }
  }
}

int main() {
  qabot::socket::Socket<SocketImpl> socket(
      qabot::socket::TransportProtocol::TCP, qabot::socket::IPVersion::IPv4);
  socket.bind("localhost", 38763);

  socket.listen(5);

  qabot::http::HttpParser httpParser;
  // Start the server accept loop
  try {
    auto serverTask = serverAcceptLoop(socket);
    std::cin.get();
  } catch (const std::exception& e) {
    std::cerr << "Error in server accept loop: " << e.what() << std::endl;
  }
}
