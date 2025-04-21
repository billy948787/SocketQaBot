#pragma once
#include <concepts>
#include <stdexcept>
#include <string>
#include <utility>

#include "type.hpp"

namespace qabot::socket {
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

  { platformImpl.init() } -> std::same_as<bool>;

  { platformImpl.accept() } -> std::same_as<PlatformImpl>;
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
  { platformImpl.getProtocol() } -> std::same_as<TransportProtocol>;
  { platformImpl.getIPVersion() } -> std::same_as<IPVersion>;
};
template <SocketImplConcept PlatformImpl>
class Socket {
 public:
  Socket(TransportProtocol protocol, IPVersion ipVersion)
      : _protocol(protocol),
        _ipVersion(ipVersion),
        _platformImpl(protocol, ipVersion) {
    _platformImpl.init();
  }
  Socket(Socket&& other) noexcept
      : _protocol(other._protocol),
        _ipVersion(other._ipVersion),
        _platformImpl(std::move(other._platformImpl)) {}

  Socket(PlatformImpl& platformImpl)
      : _protocol(platformImpl.getProtocol()),
        _ipVersion(platformImpl.getIPVersion()),
        _platformImpl(std::move(platformImpl)) {}
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
  Socket<PlatformImpl> accept() {
    auto platformImpl = _platformImpl.accept();
    return Socket<PlatformImpl>(platformImpl);
  }
  void listen(int backlog) { _platformImpl.listen(backlog); }
  void close() { _platformImpl.close(); }

 private:
  TransportProtocol _protocol;
  IPVersion _ipVersion;
  PlatformImpl _platformImpl;

  static bool inited;
  static bool init() {
    if (!inited) {
      // Initialize the platform-specific socket implementation
      inited = true;
    }
    return inited;
  }
};
}  // namespace qabot::socket