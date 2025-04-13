#pragma once
#include <winsock2.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#include <vector>

#include "socket.hpp"

namespace qabot {
class WindowsSocketImpl {
 public:
  WindowsSocketImpl(TransportProtocol protocol, IPVersion ipVersion)
      : _protocol(protocol), _ipVersion(ipVersion) {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    // Initialize socket
    _socket = socket(
        _ipVersion == IPVersion::IPv4 ? AF_INET : AF_INET6,
        _protocol == TransportProtocol::TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (_socket == INVALID_SOCKET) {
      std::cerr << "Socket error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to create socket");
    }
  }

  WindowsSocketImpl(SOCKET socket, TransportProtocol protocol,
                    IPVersion ipVersion)
      : _socket(socket), _protocol(protocol), _ipVersion(ipVersion) {}
  ~WindowsSocketImpl() {
    // Close socket
    if (_socket != INVALID_SOCKET) {
      close();
    }
  }

  WindowsSocketImpl(const WindowsSocketImpl& other) = delete;
  WindowsSocketImpl& operator=(const WindowsSocketImpl& other) = delete;

  WindowsSocketImpl(WindowsSocketImpl&& other) noexcept
      : _socket(other._socket),
        _protocol(other._protocol),
        _ipVersion(other._ipVersion) {
    other._socket = INVALID_SOCKET;  // Prevent double close
  }

  WindowsSocketImpl& operator=(WindowsSocketImpl& other) {
    if (this != &other) {
      if (_socket != INVALID_SOCKET) {
        close();
      }
      _socket = other._socket;
      _protocol = other._protocol;
      _ipVersion = other._ipVersion;

      other._socket = INVALID_SOCKET;  // Prevent double close
    }
    return *this;
  }

  void connect(const std::string& serverName, const int port) {
    addrinfo* addrInfo;
    getaddrinfo(serverName.c_str(), std::to_string(port).c_str(), nullptr,
                &addrInfo);

    if (_protocol == TransportProtocol::UDP) {
      std::cerr << "Warning: UDP does not support connect()" << std::endl;
      return;
    }
    int connectResult;

    for (addrinfo* p = addrInfo; p != nullptr; p = p->ai_next) {
      connectResult = ::connect(_socket, p->ai_addr, p->ai_addrlen);
      if (connectResult == 0) {
        std::cout << "Successfully connected to " << serverName << " ("
                  << inet_ntoa(((sockaddr_in*)p->ai_addr)->sin_addr)
                  << ") on port " << port << std::endl;
        break;  // Success
      }
    }
    if (connectResult != 0) {
      std::cerr << "Connect error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to connect to server");
    }
    freeaddrinfo(addrInfo);
  }

  void send(const std::string& message) {
    int bytesSent = ::send(_socket, message.c_str(), message.size(), 0);
    if (bytesSent == SOCKET_ERROR) {
      std::cerr << "Send error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to send message");
    }
  }

  void sendTo(const std::string& serverName, const int port,
              const std::string& message) {
    addrinfo* addrInfo;
    getaddrinfo(serverName.c_str(), std::to_string(port).c_str(), nullptr,
                &addrInfo);
    int bytesSent = 0;
    for (addrinfo* p = addrInfo; p != nullptr; p = p->ai_next) {
      bytesSent = ::sendto(_socket, message.c_str(), message.size(), 0,
                           p->ai_addr, p->ai_addrlen);
      if (bytesSent != SOCKET_ERROR) {
        std::cout << "Successfully sent message to " << serverName << " ("
                  << inet_ntoa(((sockaddr_in*)p->ai_addr)->sin_addr)
                  << ") on port " << port << std::endl;
        break;  // Success
      }
    }
    if (bytesSent == SOCKET_ERROR) {
      std::cerr << "SendTo error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to send message");
    }
    if (bytesSent != message.size()) {
      std::cerr << "Warning: Not all bytes sent" << std::endl;
    }
    freeaddrinfo(addrInfo);
  }

  void bind(const std::string& serverName, const int port) {
    addrinfo* addrInfo;
    getaddrinfo(serverName.c_str(), std::to_string(port).c_str(), nullptr,
                &addrInfo);
    int bindResult;
    for (addrinfo* p = addrInfo; p != nullptr; p = p->ai_next) {
      bindResult = ::bind(_socket, p->ai_addr, p->ai_addrlen);
      if (bindResult == 0) {
        std::cout << "Successfully bound to " << serverName << " ("
                  << inet_ntoa(((sockaddr_in*)p->ai_addr)->sin_addr)
                  << ") on port " << port << std::endl;
        break;  // Success
      }
    }
    if (bindResult != 0) {
      std::cerr << "Bind error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to bind to server");
    }
    freeaddrinfo(addrInfo);
  }

  std::string receive(size_t bufferSize) {
    std::vector<char> buffer(bufferSize);

    int bytesReceived = ::recv(_socket, buffer.data(), bufferSize, 0);
    if (bytesReceived == SOCKET_ERROR) {
      std::cerr << "Receive error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to receive message");
    }

    return std::string(buffer.data(), bytesReceived);
  }

  std::pair<std::string, ClientInfo> receiveFrom(size_t bufferSize) {
    std::vector<char> buffer(bufferSize);
    sockaddr_storage addrStorage;
    socklen_t addrLen = sizeof(addrStorage);
    int bytesReceived = ::recvfrom(_socket, buffer.data(), bufferSize, 0,
                                   (sockaddr*)&addrStorage, &addrLen);
    if (bytesReceived == SOCKET_ERROR) {
      std::cerr << "ReceiveFrom error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to receive message");
    }

    ClientInfo clientInfo;
    char clientIpStr[INET6_ADDRSTRLEN];
    if (addrStorage.ss_family == AF_INET) {
      sockaddr_in* clientAddrV4 = reinterpret_cast<sockaddr_in*>(&addrStorage);
      inet_ntop(AF_INET, &clientAddrV4->sin_addr, clientIpStr,
                sizeof(clientIpStr));
      clientInfo.port = ntohs(clientAddrV4->sin_port);
    } else if (addrStorage.ss_family == AF_INET6) {
      sockaddr_in6* clientAddrV6 =
          reinterpret_cast<sockaddr_in6*>(&addrStorage);
      inet_ntop(AF_INET6, &clientAddrV6->sin6_addr, clientIpStr,
                sizeof(clientIpStr));
      clientInfo.port = ntohs(clientAddrV6->sin6_port);
    } else {
      clientIpStr[0] = '?';
      clientIpStr[1] = '\0';
      clientInfo.port = 0;
    }
    clientInfo.ip = clientIpStr;

    return {std::string(buffer.data(), bytesReceived), clientInfo};
  }

  WindowsSocketImpl accept() {
    sockaddr_storage addrStorage;
    socklen_t addrLen = sizeof(addrStorage);
    SOCKET clientSocket = ::accept(_socket, (sockaddr*)&addrStorage, &addrLen);
    if (clientSocket == INVALID_SOCKET) {
      std::cerr << "Accept error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to accept connection");
    }

    ClientInfo clientInfo;
    char clientIpStr[INET6_ADDRSTRLEN];
    if (addrStorage.ss_family == AF_INET) {
      sockaddr_in* clientAddrV4 = reinterpret_cast<sockaddr_in*>(&addrStorage);
      inet_ntop(AF_INET, &clientAddrV4->sin_addr, clientIpStr,
                sizeof(clientIpStr));
      clientInfo.port = ntohs(clientAddrV4->sin_port);
    } else if (addrStorage.ss_family == AF_INET6) {
      sockaddr_in6* clientAddrV6 =
          reinterpret_cast<sockaddr_in6*>(&addrStorage);
      inet_ntop(AF_INET6, &clientAddrV6->sin6_addr, clientIpStr,
                sizeof(clientIpStr));
      clientInfo.port = ntohs(clientAddrV6->sin6_port);
    } else {
      clientIpStr[0] = '?';
      clientIpStr[1] = '\0';
      clientInfo.port = 0;
    }
    clientInfo.ip = clientIpStr;

    return WindowsSocketImpl(clientSocket, _protocol, _ipVersion);
  }

  void listen(int backlog) {
    int listenResult = ::listen(_socket, backlog);
    if (listenResult == SOCKET_ERROR) {
      std::cerr << "Listen error: " << WSAGetLastError() << std::endl;
      throw std::runtime_error("Failed to listen on socket");
    }
  }
  void close() {
    if (_socket != INVALID_SOCKET) {
      ::closesocket(_socket);
      _socket = INVALID_SOCKET;
    }
  }

  TransportProtocol getProtocol() const { return _protocol; }
  IPVersion getIPVersion() const { return _ipVersion; }

 private:
  TransportProtocol _protocol;
  IPVersion _ipVersion;
  SOCKET _socket;
};
}  // namespace qabot