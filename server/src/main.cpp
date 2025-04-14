#include <iostream>
#include <nlohmann/json.hpp>

#include "http/http_parser.hpp"
#include "socket/socket.hpp"
#ifdef _WIN32
#include "socket/windows_socket_impl.hpp"
using SocketImpl = qabot::socket::WindowsSocketImpl;
#else
#include "socket/unix_socket_impl.hpp"
using SocketImpl = qabot::socket::UnixSocketImpl;
#endif

int main() {
  qabot::socket::Socket<SocketImpl> socket(
      qabot::socket::TransportProtocol::TCP, qabot::socket::IPVersion::IPv4);
  socket.bind("localhost", 38763);

  socket.listen(5);

  qabot::http::HttpParser httpParser;

  while (true) {
    try {
      auto clientSocket = socket.accept();

      std::string message = clientSocket.receive(1024);

      auto request = httpParser.parseRequest(message);

      std::cout << std::endl;

      for (const auto& [key, value] :
           nlohmann::json::parse(request.body).items()) {
        std::cout << key << ": " << value << std::endl;
      }
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
    }
  }
}
