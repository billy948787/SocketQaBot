#pragma once
#include <concepts>
#include <stdexcept>
#include <string>
#include <utility>

#include "type.hpp"

namespace qabot {
enum class TransportProtocol {
  TCP,
  UDP,
};
enum class IPVersion {
  IPv4,
  IPv6,
};

template <typename PlatformImpl>
concept SocketImplConcept = requires(PlatformImpl platformImpl) {
  // Check if the constructor takes TransportProtocol and IPVersion
  {
    PlatformImpl(std::declval<TransportProtocol>(), std::declval<IPVersion>())
  };

  { platformImpl.accept() } -> std::same_as<ClientInfo>;
  { platformImpl.listen(std::declval<int>()) };
  { platformImpl.connect(std::declval<std::string>(), std::declval<int>()) };
  {
    platformImpl.sendTo(std::declval<std::string>(), std::declval<int>(),
                        std::declval<std::string>())
  };
  { platformImpl.send(std::declval<std::string>()) };
  { platformImpl.bind(std::declval<std::string>(), std::declval<int>()) };

  { platformImpl.receive(std::declval<size_t>()) } -> std::same_as<std::string>;
  {
    platformImpl.receiveFrom(std::declval<size_t>())
  } -> std::same_as<std::pair<std::string, ClientInfo>>;
  { platformImpl.close() };
};
template <SocketImplConcept PlatformImpl>
class Socket {
 public:
  Socket(TransportProtocol protocol, IPVersion ipVersion)
      : _protocol(protocol),
        _ipVersion(ipVersion),
        _platformImpl(protocol, ipVersion) {}
  ~Socket() { _platformImpl.close(); }
  void connect(const std::string& serverName, const int port) {
    _platformImpl.connect(serverName, port);
  }
  void sendTo(const std::string& serverName, const int port,
              const std::string& message) {
    _platformImpl.sendTo(serverName, port, message);
  }
  void send(const std::string& message) { _platformImpl.send(message); }

  void bind(const std::string& serverName, const int port) {
    _platformImpl.bind(serverName, port);
  }
  std::string receive(size_t bufferSize) {
    return _platformImpl.receive(bufferSize);
  }
  std::pair<std::string, ClientInfo> receiveFrom(size_t bufferSize) {
    return _platformImpl.receiveFrom(bufferSize);
  }
  ClientInfo accept() { return _platformImpl.accept(); }
  void listen(int backlog) { _platformImpl.listen(backlog); }
  void close() { _platformImpl.close(); }

 private:
  TransportProtocol _protocol;
  IPVersion _ipVersion;
  PlatformImpl _platformImpl;
};
}  // namespace qabot