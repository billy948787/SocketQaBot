#pragma once
#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

#include "socket.hpp"

namespace qabot::socket {
class UnixSocketImpl {
 public:
  UnixSocketImpl(TransportProtocol protocol, IPVersion ipVersion);

  UnixSocketImpl(const int socket, TransportProtocol protocol,
                 IPVersion ipVersion);

  ~UnixSocketImpl();

  UnixSocketImpl(const UnixSocketImpl& other) = delete;
  UnixSocketImpl& operator=(const UnixSocketImpl& other) = delete;

  UnixSocketImpl(UnixSocketImpl&& other) noexcept;

  UnixSocketImpl& operator=(UnixSocketImpl&& other);

  void connect(const std::string& serverName, const int port);

  void send(const std::string& message);

  void sendTo(const std::string& serverName, const int port,
              const std::string& message);

  void bind(const std::string& serverName, const int port);

  std::pair<std::string, ClientInfo> receiveFrom(size_t bufferSize);

  std::string receive(size_t bufferSize);

  UnixSocketImpl accept();

  void listen(int backlog);

  void close();

  bool init();

  int getSocketFD() const { return _socket; }

  TransportProtocol getProtocol() const { return _protocol; }
  IPVersion getIPVersion() const { return _ipVersion; }

 private:
  TransportProtocol _protocol;
  IPVersion _ipVersion;
  int _socket;
};
}  // namespace qabot::socket
#endif  // _WIN32