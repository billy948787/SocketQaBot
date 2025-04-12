#include <iostream>

#include "socket.hpp"
#ifdef _WIN32
#include "windows_socket_impl.hpp"
using SocketImpl = qabot::WindowsSocketImpl;
#else
#include "unix_socket_impl.hpp"
using SocketImpl = qabot::UnixSocketImpl;
#endif

int main() {
  qabot::Socket<SocketImpl> socket(qabot::TransportProtocol::TCP,
                                              qabot::IPVersion::IPv4);
  socket.bind("localhost", 38763);
  socket.listen(5);

  while (true) {
    auto clientInfo = socket.accept();
    std::cout << "Accepted connection from " << clientInfo.ip << ":"
              << clientInfo.port << std::endl;

    std::string message = socket.receive(1024);
    std::cout << "Received message: " << message << std::endl;

    socket.sendTo(clientInfo.ip, clientInfo.port, "Hello from server!");
  }
}