#include <iostream>

#include "socket.hpp"
#include "unix_socket_impl.hpp"

int main() {
  qabot::Socket<qabot::UnixSocketImpl> socket(qabot::TransportProtocol::TCP,
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