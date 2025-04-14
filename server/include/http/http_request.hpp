#pragma once
#include <nlohmann/json.hpp>

namespace qabot::http {
enum class RequestMethod {
  POST,
  GET,
  PUT,
  DELETE,
  PATCH,
  OPTIONS,
  HEAD,
};

struct HttpRequest {
 public:
  const RequestMethod method;
  const std::string path;
  const std::string body;
};

struct HttpResponse {
 public:
  const int statusCode;
  const std::string statusMessage;
  const std::string body;
};

}  // namespace qabot::http