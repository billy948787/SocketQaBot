#pragma once

#include <openssl/ssl.h>

#include "socket.hpp"
#include <vector>

namespace qabot::socket {
template <SocketImplConcept SocketImpl> class SecureSocket {
public:
  SecureSocket(TransportProtocol protocol, IPVersion ipVersion)
      : _socket(protocol, ipVersion) {
    // Initialize OpenSSL
    SSL_load_error_strings();
    // Load all algorithms
    OpenSSL_add_ssl_algorithms();
    _sslContext = SSL_CTX_new(TLS_method());
    if (!_sslContext) {
      throw std::runtime_error("Failed to create SSL context");
    }
    // forbid SSLv2 to avoid security issues
    SSL_CTX_set_options(_sslContext, SSL_OP_NO_SSLv2);
    // Create a new SSL structure for the connection
    _ssl = SSL_new(_sslContext);
    if (!_ssl) {
      throw std::runtime_error("Failed to create SSL structure");
    }
  }

  ~SecureSocket() {
    SSL_free(_ssl);
    SSL_CTX_free(_sslContext);
  }

  void connect(const std::string &host, int port) {
    _socket.connect(host, port);
    SSL_set_fd(_ssl, _socket.getSocketFD());
    if (SSL_connect(_ssl) <= 0) {
      throw std::runtime_error("Failed to establish SSL connection");
    }
  }

  void send(const std::string &data) {
    if (SSL_write(_ssl, data.c_str(), data.size()) <= 0) {
      throw std::runtime_error("Failed to send data");
    }
  }

  std::string receive(size_t size) {
    std::vector<char> buffer(size);
    int bytesReceived = SSL_read(_ssl, buffer.data(), size);
    if (bytesReceived <= 0) {
      throw std::runtime_error("Failed to receive data");
    }
    return std::string(buffer.data(), bytesReceived);
  }

private:
  SocketImpl _socket;
  SSL_CTX *_sslContext;
  SSL *_ssl;
};
} // namespace qabot::socket