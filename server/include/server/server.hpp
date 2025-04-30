#pragma once
#include "task/task.hpp"
#ifdef _WIN32
#include "socket/windows_socket_impl.hpp"
using SocketImpl = qabot::socket::WindowsSocketImpl;
#else
#include "socket/unix_socket_impl.hpp"
using SocketImpl = qabot::socket::UnixSocketImpl;
#endif

namespace qabot::server {
class Server {
 public:
  Server()
      : _serverSocket(qabot::socket::TransportProtocol::TCP,
                      qabot::socket::IPVersion::IPv4) {}
  // singleton
  static Server& getInstance() {
    static Server instance;
    return instance;
  }

  // 禁止複製和移動
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(Server&&) = delete;
  void start();

 private:
  qabot::task::Task<void> _serverLoop();
  qabot::task::Task<void> _clientLoop(
      qabot::socket::Socket<SocketImpl>&& clientSocket);

  qabot::socket::Socket<SocketImpl> _serverSocket;
};
}  // namespace qabot::server