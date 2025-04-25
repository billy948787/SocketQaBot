#pragma once
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "http.hpp"

namespace qabot::http {
inline std::string serializeRequest(
    const RequestMethod method, const std::string& url,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& body) {
  if (method == RequestMethod::POST &&
      headers.find("Content-Type") == headers.end()) {
    throw std::runtime_error("POST request must have Content-Type header");
  }
  std::stringstream requestStream;
  std::string methodStr = requestMethodToString(method);
  requestStream << methodStr << " " << url << " HTTP/1.1\r\n";
  for (const auto& [key, value] : headers) {
    requestStream << key << ": " << value << "\r\n";
  }
  if (!body.empty()) {
    requestStream << "Content-Length: " << body.size() << "\r\n";
    requestStream << "\r\n" << body;
  }
  return requestStream.str();
}

inline std::string serializeResponse(
    const ResponseStatus statusCode,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& body) {
  std::stringstream responseStream;
  std::string statusStr = responseStatusToString(statusCode);

  responseStream << "HTTP/1.1 " << statusStr << "\r\n";
  for (const auto& [key, value] : headers) {
    responseStream << key << ": " << value << "\r\n";
  }
  responseStream << "Content-Length: " << body.size() << "\r\n";
  responseStream << "\r\n" << body;
  return responseStream.str();
}
}  // namespace qabot::http