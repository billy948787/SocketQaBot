#pragma once
#include <exception>
#include <string>

namespace qabot::socket {
class SocketException : public std::exception {
public:
  explicit SocketException(int statusCode, const std::string &message)
      : _message(message), _statusCode(statusCode) {}

  const char *what() const noexcept override { return _message.c_str(); }

  int statusCode() const noexcept { return _statusCode; }

  

private:
  std::string _message;

  int _statusCode; // Default status code
};
} // namespace qabot::socket