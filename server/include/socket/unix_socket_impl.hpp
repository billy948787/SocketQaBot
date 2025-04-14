#pragma once
#ifndef _WIN32
#include <arpa/inet.h>
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
  UnixSocketImpl(TransportProtocol protocol, IPVersion ipVersion)
      : _protocol(protocol), _ipVersion(ipVersion) {
    // Initialize socket
    _socket = ::socket(
        _ipVersion == IPVersion::IPv4 ? AF_INET : AF_INET6,
        _protocol == TransportProtocol::TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (_socket < 0) {
      throw std::runtime_error("Failed to create socket");
    }
  }

  UnixSocketImpl(const int socket, TransportProtocol protocol,
                 IPVersion ipVersion)
      : _socket(socket), _protocol(protocol), _ipVersion(ipVersion) {}

  ~UnixSocketImpl() {
    // Close socket
    if (_socket >= 0) {
      close();
    }
  }

  UnixSocketImpl(const UnixSocketImpl& other) = delete;
  UnixSocketImpl& operator=(const UnixSocketImpl& other) = delete;

  UnixSocketImpl(UnixSocketImpl&& other) noexcept
      : _socket(other._socket),
        _protocol(other._protocol),
        _ipVersion(other._ipVersion) {
    other._socket = -1;  // Prevent double close
  }

  UnixSocketImpl& operator=(UnixSocketImpl&& other) {
    if (this != &other) {
      if (_socket >= 0) {
        close();
      }
      _socket = other._socket;
      _protocol = other._protocol;
      _ipVersion = other._ipVersion;

      other._socket = -1;  // Prevent double close
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
      std::cerr << "Connect error: " << strerror(errno) << std::endl;
      throw std::runtime_error("Failed to connect to server");
    }
    freeaddrinfo(addrInfo);
  }

  void send(const std::string& message) {
    ssize_t bytesSent = ::send(_socket, message.c_str(), message.size(), 0);
    if (bytesSent < 0) {
      std::cerr << "Send error: " << strerror(errno) << std::endl;
      throw std::runtime_error("Failed to send message");
    }
  }

  void sendTo(const std::string& serverName, const int port,
              const std::string& message) {
    addrinfo* addrInfo;
    getaddrinfo(serverName.c_str(), std::to_string(port).c_str(), nullptr,
                &addrInfo);
    ssize_t bytesSent = 0;
    for (addrinfo* p = addrInfo; p != nullptr; p = p->ai_next) {
      bytesSent = ::sendto(_socket, message.c_str(), message.size(), 0,
                           p->ai_addr, p->ai_addrlen);
      if (bytesSent >= 0) {
        std::cout << "Successfully sent message to " << serverName << " ("
                  << inet_ntoa(((sockaddr_in*)p->ai_addr)->sin_addr)
                  << ") on port " << port << std::endl;
        break;  // Success
      }
    }
    if (bytesSent < 0) {
      std::cerr << "SendTo error: " << strerror(errno) << std::endl;
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
      std::cerr << "Bind error: " << strerror(errno) << std::endl;
      throw std::runtime_error("Failed to bind to server");
    }
    freeaddrinfo(addrInfo);
  }

  std::pair<std::string, ClientInfo> receiveFrom(size_t bufferSize) {
    std::vector<char> buffer(bufferSize);
    sockaddr_storage addrStorage;
    socklen_t addrLen = sizeof(addrStorage);
    ssize_t bytesReceived = ::recvfrom(_socket, buffer.data(), bufferSize, 0,
                                       (sockaddr*)&addrStorage, &addrLen);
    if (bytesReceived < 0) {
      std::cerr << "ReceiveFrom error: " << strerror(errno) << std::endl;
      throw std::runtime_error("Failed to receive message");
    }

    std::string message(buffer.data(), bytesReceived);
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

    return {message, clientInfo};
  }

  std::string receive(size_t bufferSize) {
    std::vector<char> buffer(bufferSize);

    ssize_t bytesReceived = ::recv(_socket, buffer.data(), bufferSize, 0);
    if (bytesReceived < 0) {
      std::cerr << "Receive error: " << strerror(errno) << std::endl;
      throw std::runtime_error("Failed to receive message");
    }

    return std::string(buffer.data(), bytesReceived);
  }

  UnixSocketImpl accept() {
    sockaddr_storage addrStorage;
    socklen_t addrLen = sizeof(addrStorage);
    int clientSocket = ::accept(_socket, (sockaddr*)&addrStorage, &addrLen);
    if (clientSocket < 0) {
      std::cerr << "Accept error: " << strerror(errno) << std::endl;
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
    std::cout << "Accepted connection from " << clientInfo.ip << ":"
              << clientInfo.port << std::endl;

    return UnixSocketImpl(clientSocket, _protocol, _ipVersion);
  }

  void listen(int backlog) {
    int listenResult = ::listen(_socket, backlog);
    if (listenResult < 0) {
      std::cerr << "Listen error: " << strerror(errno) << std::endl;
      throw std::runtime_error("Failed to listen on socket");
    }
  }

  void close() {
    if (_socket >= 0) {
      shutdown(_socket, SHUT_RDWR);
      _socket = -1;
    }
    std::cout << "Socket closed." << std::endl;
  }

  TransportProtocol getProtocol() const { return _protocol; }
  IPVersion getIPVersion() const { return _ipVersion; }

 private:
  TransportProtocol _protocol;
  IPVersion _ipVersion;
  int _socket;
};
}  // namespace qabot
#endif  // _WIN32