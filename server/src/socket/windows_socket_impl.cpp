#ifdef _WIN32
#include "socket/windows_socket_impl.hpp"

namespace qabot::socket {
std::mutex WindowsSocketImpl::_socketMutex;
int WindowsSocketImpl::_activeSocketInstance = 0;

// function implementation
WindowsSocketImpl::WindowsSocketImpl(TransportProtocol protocol,
                                     IPVersion ipVersion)
    : _protocol(protocol), _ipVersion(ipVersion) {
  std::lock_guard<std::mutex> lock(_socketMutex);

  if (_activeSocketInstance == 0) {
    // Initialize Winsock
    WSADATA wsaData;

    auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0) {
      std::cerr << "WSAStartup failed: " << result << std::endl;
      throw std::runtime_error("Failed to initialize Winsock");
    }
  }
  // Initialize socket
  _socket = ::socket(
      _ipVersion == IPVersion::IPv4 ? AF_INET : AF_INET6,
      _protocol == TransportProtocol::TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
  if (_socket == INVALID_SOCKET) {
    std::cerr << "Socket error: " << WSAGetLastError() << std::endl;
    throw std::runtime_error("Failed to create socket");
  }
  _activeSocketInstance++;
  init();
}

WindowsSocketImpl::WindowsSocketImpl(SOCKET socket, TransportProtocol protocol,
                                     IPVersion ipVersion)
    : _socket(socket), _protocol(protocol), _ipVersion(ipVersion) {
  std::lock_guard<std::mutex> lock(_socketMutex);
  _activeSocketInstance++;
  init();
}

WindowsSocketImpl::~WindowsSocketImpl() {
  // Close socket
  if (_socket != INVALID_SOCKET) {
    close();
    std::lock_guard<std::mutex> lock(_socketMutex);
    _activeSocketInstance--;
    if (_activeSocketInstance == 0) {
      // Cleanup Winsock
      WSACleanup();
    }
  }
}
bool WindowsSocketImpl::init() {
  // set to non-blocking mode
  u_long mode = 1;
  if (ioctlsocket(_socket, FIONBIO, &mode) != 0) {
    std::cerr << "Failed to set non-blocking mode: " << WSAGetLastError()
              << std::endl;
    return false;
  }
  return true;
}

void WindowsSocketImpl::connect(const std::string &serverName, const int port) {
  addrinfo *addrInfo;
  auto result = getaddrinfo(serverName.c_str(), std::to_string(port).c_str(),
                            nullptr, &addrInfo);

  if (result != 0) {
    std::cerr << "getaddrinfo failed for " << serverName << ":" << port
              << " - Error (" << result << "): " << std::endl;

    throw std::system_error(WSAGetLastError(), std::generic_category(),
                            "Failed to get address info for " + serverName +
                                ":" + std::to_string(port));
  }

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
    throw std::system_error(WSAGetLastError(), std::generic_category(),
                            "Failed to connect to " + serverName + ":" +
                                std::to_string(port));
  }
  freeaddrinfo(addrInfo);
}

void WindowsSocketImpl::send(const std::string &message) {
  int bytesSent = ::send(_socket, message.c_str(), message.size(), 0);
  if (bytesSent == SOCKET_ERROR) {
    std::cerr << "Send error: " << WSAGetLastError() << std::endl;
    throw std::runtime_error("Failed to send message");
  }
}

void WindowsSocketImpl::sendTo(const std::string &serverName, const int port,
                               const std::string &message) {
  addrinfo *addrInfo;
  getaddrinfo(serverName.c_str(), std::to_string(port).c_str(), nullptr,
              &addrInfo);
  int bytesSent = 0;
  for (addrinfo *p = addrInfo; p != nullptr; p = p->ai_next) {
    bytesSent = ::sendto(_socket, message.c_str(), message.size(), 0,
                         p->ai_addr, p->ai_addrlen);
    if (bytesSent != SOCKET_ERROR) {
      std::cout << "Successfully sent message to " << serverName << " ("
                << inet_ntoa(((sockaddr_in *)p->ai_addr)->sin_addr)
                << ") on port " << port << std::endl;
      break; // Success
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

void WindowsSocketImpl::bind(const std::string &serverName, const int port) {
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
    std::cerr << "Bind error: " << WSAGetLastError() << std::endl;
    throw std::runtime_error("Failed to bind to server");
  }
  freeaddrinfo(addrInfo);
}

std::string WindowsSocketImpl::receive(size_t bufferSize) {
  std::vector<char> buffer(bufferSize);

  int bytesReceived = ::recv(_socket, buffer.data(), bufferSize, 0);
  if (bytesReceived == SOCKET_ERROR) {
    throw std::system_error(WSAGetLastError(), std::generic_category(),
                            "Failed to receive message");
  }

  return std::string(buffer.data(), bytesReceived);
}

std::pair<std::string, ClientInfo>
WindowsSocketImpl::receiveFrom(size_t bufferSize) {
  std::vector<char> buffer(bufferSize);
  sockaddr_storage addrStorage;
  socklen_t addrLen = sizeof(addrStorage);
  int bytesReceived = ::recvfrom(_socket, buffer.data(), bufferSize, 0,
                                 (sockaddr *)&addrStorage, &addrLen);
  if (bytesReceived == SOCKET_ERROR) {
    throw std::system_error(WSAGetLastError(), std::generic_category(),
                            "Failed to receive message");
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

  return {std::string(buffer.data(), bytesReceived), clientInfo};
}

WindowsSocketImpl WindowsSocketImpl::accept() {
  sockaddr_storage addrStorage;
  socklen_t addrLen = sizeof(addrStorage);
  SOCKET clientSocket = ::accept(_socket, (sockaddr *)&addrStorage, &addrLen);
  if (clientSocket == INVALID_SOCKET) {
    throw std::system_error(WSAGetLastError(), std::generic_category(),
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

  return WindowsSocketImpl(clientSocket, _protocol, _ipVersion);
}

void WindowsSocketImpl::listen(int backlog) {
  int listenResult = ::listen(_socket, backlog);
  if (listenResult == SOCKET_ERROR) {
    std::cerr << "Listen error: " << WSAGetLastError() << std::endl;
    throw std::runtime_error("Failed to listen on socket");
  }
}

void WindowsSocketImpl::close() {
  if (_socket != INVALID_SOCKET) {
    ::closesocket(_socket);
    _socket = INVALID_SOCKET;

    std::cout << "Socket closed" << std::endl;
  }
}

} // namespace qabot::socket
#endif