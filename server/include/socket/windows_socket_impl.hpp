#pragma once
#ifdef _WIN32
#include <mutex>
#include <winsock2.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#include <iostream>
#include <vector>

#include "socket.hpp"

namespace qabot::socket {
class WindowsSocketImpl {
public:
  WindowsSocketImpl(TransportProtocol protocol, IPVersion ipVersion);

  WindowsSocketImpl(SOCKET socket, TransportProtocol protocol,
                    IPVersion ipVersion);
  ~WindowsSocketImpl();

  bool init();

  WindowsSocketImpl(const WindowsSocketImpl &other) = delete;
  WindowsSocketImpl &operator=(const WindowsSocketImpl &other) = delete;

  WindowsSocketImpl(WindowsSocketImpl &&other) noexcept
      : _socket(other._socket), _protocol(other._protocol),
        _ipVersion(other._ipVersion) {
    other._socket = INVALID_SOCKET; // Prevent double close
  }

  WindowsSocketImpl &operator=(WindowsSocketImpl &other) {
    if (this != &other) {
      if (_socket != INVALID_SOCKET) {
        close();
      }
      _socket = other._socket;
      _protocol = other._protocol;
      _ipVersion = other._ipVersion;

      other._socket = INVALID_SOCKET; // Prevent double close
    }
    return *this;
  }

  void connect(const std::string &serverName, const int port);

  void send(const std::string &message);

  void sendTo(const std::string &serverName, const int port,
              const std::string &message);

  void bind(const std::string &serverName, const int port);

  std::string receive(size_t bufferSize);

  std::pair<std::string, ClientInfo> receiveFrom(size_t bufferSize);

  WindowsSocketImpl accept();

  void listen(int backlog);
  void close();

  TransportProtocol getProtocol() const { return _protocol; }
  IPVersion getIPVersion() const { return _ipVersion; }

  SOCKET getSocketFD() const { return _socket; }

private:
  TransportProtocol _protocol;
  IPVersion _ipVersion;
  SOCKET _socket;

  static int _activeSocketInstance;
  static std::mutex _socketMutex;
};
} // namespace qabot::socket
#endif // _WIN32