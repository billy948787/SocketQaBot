#include <iostream>

#include "socket.hpp"
#include "unix_socket_impl.hpp"

int main() {
  qabot::Socket<qabot::UnixSocketImpl> socket(qabot::TransportProtocol::TCP,
                                              qabot::IPVersion::IPv4);

  try {
    socket.connect("youtube.com", 443);
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}