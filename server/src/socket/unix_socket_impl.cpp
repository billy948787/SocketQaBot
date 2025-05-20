#ifndef _WIN32
#include "socket/unix_socket_impl.hpp"

using namespace qabot::socket;

UnixSocketImpl::UnixSocketImpl(TransportProtocol protocol, IPVersion ipVersion)
    : _protocol(protocol), _ipVersion(ipVersion) {
  // Initialize socket
  _socket = ::socket(
      _ipVersion == IPVersion::IPv4 ? AF_INET : AF_INET6,
      _protocol == TransportProtocol::TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
  if (_socket < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  init();
}

UnixSocketImpl::UnixSocketImpl(const int socket, TransportProtocol protocol,
                               IPVersion ipVersion)
    : _socket(socket), _protocol(protocol), _ipVersion(ipVersion) {
  init();
}

UnixSocketImpl::~UnixSocketImpl() {
  // Close socket
  if (_socket >= 0) {
    close();
  }
}

UnixSocketImpl::UnixSocketImpl(UnixSocketImpl &&other) noexcept
    : _socket(other._socket), _protocol(other._protocol),
      _ipVersion(other._ipVersion) {
  other._socket = -1; // Prevent double close
}

UnixSocketImpl &UnixSocketImpl::operator=(UnixSocketImpl &&other) {
  if (this != &other) {
    if (_socket >= 0) {
      close();
    }
    _socket = other._socket;
    _protocol = other._protocol;
    _ipVersion = other._ipVersion;

    other._socket = -1; // Prevent double close
  }
  return *this;
}

void UnixSocketImpl::connect(const std::string &serverName, const int port) {
  addrinfo *addrInfo;
  getaddrinfo(serverName.c_str(), std::to_string(port).c_str(), nullptr,
              &addrInfo);
  if (_protocol == TransportProtocol::UDP) {
    std::cerr << "Warning: UDP does not support connect()" << std::endl;
    return;
  }

  int connectResult;

  for (addrinfo *p = addrInfo; p != nullptr; p = p->ai_next) {
    connectResult = ::connect(_socket, p->ai_addr, p->ai_addrlen);
    if (connectResult == 0) {
      std::cout << "Successfully connected to " << serverName << " ("
                << inet_ntoa(((sockaddr_in *)p->ai_addr)->sin_addr)
                << ") on port " << port << std::endl;
      break; // Success
    }
  }

  if (connectResult != 0) {
    if (errno == EISCONN) {
    } else {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to connect to server: " + serverName +
                                  ":" + std::to_string(port));
    }
  }
  freeaddrinfo(addrInfo);
}

void UnixSocketImpl::send(const std::string &message) {
  ssize_t bytesSent = ::send(_socket, message.c_str(), message.size(), 0);
  if (bytesSent < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to send message");
  }
}

void UnixSocketImpl::sendTo(const std::string &serverName, const int port,
                            const std::string &message) {
  addrinfo *addrInfo;
  getaddrinfo(serverName.c_str(), std::to_string(port).c_str(), nullptr,
              &addrInfo);
  ssize_t bytesSent = 0;
  for (addrinfo *p = addrInfo; p != nullptr; p = p->ai_next) {
    bytesSent = ::sendto(_socket, message.c_str(), message.size(), 0,
                         p->ai_addr, p->ai_addrlen);
    if (bytesSent >= 0) {
      std::cout << "Successfully sent message to " << serverName << " ("
                << inet_ntoa(((sockaddr_in *)p->ai_addr)->sin_addr)
                << ") on port " << port << std::endl;
      break; // Success
    }
  }
  if (bytesSent < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to send message to " + serverName + ":" +
                                std::to_string(port));
  }
  if (bytesSent != message.size()) {
    std::cerr << "Warning: Not all bytes sent" << std::endl;
  }
  freeaddrinfo(addrInfo);
}

void UnixSocketImpl::bind(const std::string &serverName, const int port) {
  addrinfo *addrInfo;
  getaddrinfo(serverName.c_str(), std::to_string(port).c_str(), nullptr,
              &addrInfo);
  int bindResult;
  for (addrinfo *p = addrInfo; p != nullptr; p = p->ai_next) {
    bindResult = ::bind(_socket, p->ai_addr, p->ai_addrlen);
    if (bindResult == 0) {
      std::cout << "Successfully bound to " << serverName << " ("
                << inet_ntoa(((sockaddr_in *)p->ai_addr)->sin_addr)
                << ") on port " << port << std::endl;
      break; // Success
    }
  }
  if (bindResult != 0) {
    std::cerr << "Bind error: " << strerror(errno) << std::endl;
    throw std::runtime_error("Failed to bind to server");
  }
  freeaddrinfo(addrInfo);
}

std::pair<std::string, ClientInfo>
UnixSocketImpl::receiveFrom(size_t bufferSize) {
  std::vector<char> buffer(bufferSize);
  sockaddr_storage addrStorage;
  socklen_t addrLen = sizeof(addrStorage);
  ssize_t bytesReceived = ::recvfrom(_socket, buffer.data(), bufferSize, 0,
                                     (sockaddr *)&addrStorage, &addrLen);
  if (bytesReceived < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to receive message");
  }

  std::string message(buffer.data(), bytesReceived);
  ClientInfo clientInfo;
  char clientIpStr[INET6_ADDRSTRLEN];

  if (addrStorage.ss_family == AF_INET) {
    sockaddr_in *clientAddrV4 = reinterpret_cast<sockaddr_in *>(&addrStorage);
    inet_ntop(AF_INET, &clientAddrV4->sin_addr, clientIpStr,
              sizeof(clientIpStr));
    clientInfo.port = ntohs(clientAddrV4->sin_port);
  } else if (addrStorage.ss_family == AF_INET6) {
    sockaddr_in6 *clientAddrV6 = reinterpret_cast<sockaddr_in6 *>(&addrStorage);
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

std::string UnixSocketImpl::receive(size_t bufferSize) {
  std::vector<char> buffer(bufferSize);

  ssize_t bytesReceived = ::recv(_socket, buffer.data(), bufferSize, 0);
  if (bytesReceived < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to receive message");
  }

  return std::string(buffer.data(), bytesReceived);
}

UnixSocketImpl UnixSocketImpl::accept() {
  sockaddr_storage addrStorage;
  socklen_t addrLen = sizeof(addrStorage);
  int clientSocket = ::accept(_socket, (sockaddr *)&addrStorage, &addrLen);
  if (clientSocket < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to accept connection");
  }
  ClientInfo clientInfo;
  char clientIpStr[INET6_ADDRSTRLEN];
  if (addrStorage.ss_family == AF_INET) {
    sockaddr_in *clientAddrV4 = reinterpret_cast<sockaddr_in *>(&addrStorage);
    inet_ntop(AF_INET, &clientAddrV4->sin_addr, clientIpStr,
              sizeof(clientIpStr));
    clientInfo.port = ntohs(clientAddrV4->sin_port);
  } else if (addrStorage.ss_family == AF_INET6) {
    sockaddr_in6 *clientAddrV6 = reinterpret_cast<sockaddr_in6 *>(&addrStorage);
    inet_ntop(AF_INET6, &clientAddrV6->sin6_addr, clientIpStr,
              sizeof(clientIpStr));
    clientInfo.port = ntohs(clientAddrV6->sin6_port);
  } else {
    clientIpStr[0] = '?';
    clientIpStr[1] = '\0';
    clientInfo.port = 0;
  }
  clientInfo.ip = clientIpStr;

  return UnixSocketImpl(clientSocket, _protocol, _ipVersion);
}

void UnixSocketImpl::listen(int backlog) {
  int listenResult = ::listen(_socket, backlog);
  if (listenResult < 0) {
    std::cerr << "Listen error: " << strerror(errno) << std::endl;
    throw std::runtime_error("Failed to listen on socket");
  }
}

void UnixSocketImpl::close() {
  if (_socket >= 0) {
    shutdown(_socket, SHUT_RDWR);
    _socket = -1;
  }
}

bool UnixSocketImpl::init() {
  const int flags = fcntl(_socket, F_GETFL, 0);
  if (flags == -1) {
    std::cerr << "Failed to get socket flags: " << strerror(errno) << std::endl;
    return false;
  }

  if (flags & O_NONBLOCK) {
    return true;
  }

  if (fcntl(_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
    std::cerr << "Failed to set socket to non-blocking: " << strerror(errno)
              << std::endl;
    return false;
  }

  // std::cout << "Socket set to non-blocking mode." << std::endl;
  return true;
}
#endif