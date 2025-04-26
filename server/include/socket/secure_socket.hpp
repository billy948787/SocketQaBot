#pragma once

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <mutex>

#include "socket.hpp"

const char* get_ssl_state_string(const SSL* ssl) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  return SSL_state_string_long(ssl);
#else
  // Older OpenSSL might not have the long version readily available
  // or might behave differently. This is a basic fallback.
  return SSL_state_string(ssl);
#endif
}

namespace qabot::socket {
template <SocketImplConcept SocketImpl>
class SecureSocket {
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

  SecureSocket(SecureSocket&& other) noexcept
      : _socket(std::move(other._socket)),
        _sslContext(other._sslContext),
        _ssl(other._ssl) {
    other._sslContext = nullptr;
    other._ssl = nullptr;
  }

  SecureSocket& operator=(SecureSocket&& other) {
    if (this != &other) {
      SSL_free(_ssl);
      SSL_CTX_free(_sslContext);
      _socket = std::move(other._socket);
      _sslContext = other._sslContext;
      _ssl = other._ssl;
      other._sslContext = nullptr;
      other._ssl = nullptr;
    }
    return *this;
  }

  SecureSocket(const SecureSocket&) = delete;
  SecureSocket& operator=(const SecureSocket&) = delete;

  ~SecureSocket() {
    std::lock_guard<std::mutex> lock(_sslMutex);
    if (_ssl) {
      SSL_free(_ssl);
    }
    if (_sslContext) {
      SSL_CTX_free(_sslContext);
    }
  }

  void connect(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(_sslMutex);  // Lock for thread safety
    _socket.connect(host, port);
    SSL_set_fd(_ssl, _socket.getSocketFD());
    int connectResult = SSL_connect(_ssl);  // Store result
    if (connectResult <= 0) {
      int sslError =
          SSL_get_error(_ssl, connectResult);  // Get specific SSL error
      unsigned long errCode = ERR_get_error();
      char errBuf[256];
      ERR_error_string_n(errCode, errBuf, sizeof(errBuf));

      std::string errorMsg =
          "Failed to establish SSL connection (SSL_connect error code: " +
          std::to_string(sslError) + ", reason: " + std::string(errBuf) + ")";

      // Add specific handling if needed, e.g., for SSL_ERROR_SYSCALL
      if (sslError == SSL_ERROR_SYSCALL && errCode == 0) {
        errorMsg +=
            " - Underlying socket error: " + std::string(strerror(errno));
      }

      throw std::runtime_error(errorMsg);  // Throw detailed error
    }
    // Optional: Log success or certificate details here if needed
    std::cout << "SSL connection established successfully." << std::endl;
    std::cout << "Final SSL state in connect: " << get_ssl_state_string(_ssl)
              << std::endl;  // Log final state in connect
    std::cout << "SSL* address in connect: " << _ssl
              << std::endl;  // <-- Add this line
  }

  void send(const std::string& data) {
    std::lock_guard<std::mutex> lock(_sslMutex);  // Lock for thread safety
    std::cout << "SSL state before send: " << get_ssl_state_string(_ssl)
              << std::endl;  // Log state immediately before send
    std::cout << "SSL* address before send: " << _ssl << std::endl;
    int bytesWritten = SSL_write(
        _ssl, data.c_str(), static_cast<int>(data.size()));  // Cast size to int

    if (bytesWritten <= 0) {
      int sslError =
          SSL_get_error(_ssl, bytesWritten);  // Get specific SSL error code
      unsigned long errCode =
          ERR_get_error();  // Get last error code from the error queue
      char errBuf[256];     // Buffer for error string
      ERR_error_string_n(errCode, errBuf,
                         sizeof(errBuf));  // Get human-readable error string

      // Construct detailed error message
      std::string errorMsg = "Failed to send data (SSL_write error code: " +
                             std::to_string(sslError) +
                             ", reason: " + std::string(errBuf) + ")";

      // Specific handling for common errors (optional but helpful)
      if (sslError == SSL_ERROR_ZERO_RETURN) {
        errorMsg += " - Connection closed cleanly by peer.";
      } else if (sslError == SSL_ERROR_SYSCALL) {
        // Check errno if SSL_ERROR_SYSCALL and errCode is 0
        if (errCode == 0) {
          errorMsg +=
              " - Underlying socket error (check errno/WSAGetLastError): " +
              std::string(strerror(errno));
        } else {
          errorMsg += " - Underlying socket error (SSL_ERROR_SYSCALL).";
        }
      }

      throw std::runtime_error(errorMsg);
    }
    // Optional: Check if all data was written (SSL_write might write less than
    // requested) if (static_cast<size_t>(bytesWritten) != data.size()) {
    //    // Handle partial write - potentially loop or throw error
    //    throw std::runtime_error("Partial data sent via SSL_write.");
    // }
  }

  std::string receive(size_t size) {
    std::stringstream ss_thread;
    ss_thread << std::this_thread::get_id();
    std::cerr << "[" << ss_thread.str()
              << "] SecureSocket::receive: START. this=" << this
              << ", &_sslMutex=" << &_sslMutex << ", _ssl=" << _ssl
              << std::endl;  // Log this pointer and mutex address

    if (!_ssl) {
      std::cerr << "[" << ss_thread.str()
                << "] SecureSocket::receive: FATAL ERROR! _ssl pointer is NULL "
                   "before locking mutex!"
                << std::endl;
      throw std::runtime_error(
          "SecureSocket::receive called with null _ssl pointer");
    }

    std::cerr << "[" << ss_thread.str()
              << "] SecureSocket::receive: Attempting to lock mutex at address "
              << &_sslMutex << " for SSL* " << _ssl
              << std::endl;  // Log address again before lock

    try {
      std::lock_guard<std::mutex> lock(_sslMutex);  // <<< CRASH LIKELY HERE
      std::cerr << "[" << ss_thread.str()
                << "] SecureSocket::receive: Mutex locked for SSL* " << _ssl
                << std::endl;

      if (!_ssl) {
        std::cerr << "[" << ss_thread.str()
                  << "] SecureSocket::receive: FATAL ERROR! _ssl pointer is "
                     "NULL after locking mutex!"
                  << std::endl;
        throw std::runtime_error(
            "SecureSocket::receive called with null _ssl pointer (after lock)");
      }

      std::vector<char> buffer(size);
      std::cerr << "[" << ss_thread.str()
                << "] SecureSocket::receive: Calling SSL_read for SSL* " << _ssl
                << std::endl;
      int bytesReceived =
          SSL_read(_ssl, buffer.data(), static_cast<int>(buffer.size()));

      // ... rest of the function ...
      if (bytesReceived > 0) {
        std::cerr << "[" << ss_thread.str()
                  << "] SecureSocket::receive: SSL_read success, bytes="
                  << bytesReceived << std::endl;
        return std::string(buffer.data(), bytesReceived);
      } else {
        // ... error handling ...
        int sslError = SSL_get_error(_ssl, bytesReceived);
        std::string errorMsg = "[" + ss_thread.str() +
                               "] SecureSocket::receive: SSL_read error: " +
                               std::to_string(sslError);
        ERR_print_errors_fp(stderr);         // Print OpenSSL error stack
        std::cerr << errorMsg << std::endl;  // Also print to cerr
        // Decide whether to throw or return empty based on error
        if (sslError == SSL_ERROR_ZERO_RETURN ||
            sslError == SSL_ERROR_SYSCALL) {
          return "";  // Connection closed or underlying socket error
        }
        throw std::runtime_error(errorMsg);
      }

    } catch (const std::exception& lock_ex) {
      // Catch potential exceptions during locking itself, though unlikely for
      // segfault
      std::cerr
          << "[" << ss_thread.str()
          << "] SecureSocket::receive: EXCEPTION during mutex lock! what(): "
          << lock_ex.what() << std::endl;
      throw;  // Rethrow
    }
  }

 private:
  SocketImpl _socket;
  SSL_CTX* _sslContext;
  SSL* _ssl;
  std::mutex _sslMutex;  // Mutex for thread safety
};
}  // namespace qabot::socket