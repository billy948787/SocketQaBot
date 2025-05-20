#pragma once

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "socket.hpp"
#include <cerrno>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <system_error>
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
    SSL_set_fd(_ssl, _socket.getSocketFD());
    _socket.connect(host, port);
    ERR_clear_error();
    auto ret = SSL_connect(_ssl);

    if (ret <= 0) {
      if (SSL_get_error(_ssl, ret) == SSL_ERROR_WANT_WRITE ||
          SSL_get_error(_ssl, ret) == SSL_ERROR_WANT_READ) {
        // Handle non-blocking connect
        throw std::system_error{
            static_cast<int>(std::errc::operation_would_block),
            std::generic_category(), "Non-blocking connect would block"};
      } else {
        std::cerr << "SSL error: " << SSL_get_error(_ssl, ret) << std::endl;
        throw std::runtime_error("Failed to establish SSL connection");
      }
    }
  }

  void send(const std::string &data) {
    ERR_clear_error();
    auto ret = SSL_write(_ssl, data.c_str(), data.size());
    if (ret <= 0) {
      if (SSL_get_error(_ssl, ret) == SSL_ERROR_WANT_WRITE ||
          SSL_get_error(_ssl, ret) == SSL_ERROR_WANT_READ) {
        // Handle non-blocking write
        throw std::system_error{
            static_cast<int>(std::errc::operation_would_block),
            std::generic_category(), "Non-blocking write would block"};
      } else {
        long error_code = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(error_code, err_buf, sizeof(err_buf));
        std::cerr << "SSL_write error: " << SSL_get_error(_ssl, ret)
                  << ", OpenSSL error: " << err_buf << std::endl;
        throw std::runtime_error(std::string("Failed to send data over SSL: ") +
                                 err_buf);
      }
    }
  }

  std::string receive(size_t size) {
    std::vector<char> buffer(size);
    ERR_clear_error();
    int ret = SSL_read(_ssl, buffer.data(), size);
    if (ret <= 0) {
      if (SSL_get_error(_ssl, ret) == SSL_ERROR_WANT_READ ||
          SSL_get_error(_ssl, ret) == SSL_ERROR_WANT_WRITE) {
        // Handle non-blocking read
        throw std::system_error{
            static_cast<int>(std::errc::operation_would_block),
            std::generic_category(), "Non-blocking read would block"};
      } else {
        std::cerr << "SSL error: " << SSL_get_error(_ssl, ret) << std::endl;
        throw std::runtime_error("Failed to receive data over SSL");
      }
    }
    return std::string(buffer.data(), ret);
  }

private:
  SocketImpl _socket;
  SSL_CTX *_sslContext;
  SSL *_ssl;
};
} // namespace qabot::socket