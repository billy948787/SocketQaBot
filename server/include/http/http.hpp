#pragma once
#include <nlohmann/json.hpp>

#include "socket/socket.hpp"

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

enum class ResponseStatus {
  OK = 200,
  Created = 201,
  Accepted = 202,
  NoContent = 204,
  BadRequest = 400,
  Unauthorized = 401,
  Forbidden = 403,
  NotFound = 404,
  InternalServerError = 500,
};
enum class ContentType {
  JSON,
  HTML,
  TEXT,
  XML,
};

inline std::string contentTypeToString(ContentType type) {
  switch (type) {
    case ContentType::JSON:
      return "application/json";
    case ContentType::HTML:
      return "text/html";
    case ContentType::TEXT:
      return "text/plain";
    case ContentType::XML:
      return "application/xml";
    default:
      return "application/octet-stream";
  }
}

inline std::string requestMethodToString(RequestMethod method) {
  switch (method) {
    case RequestMethod::POST:
      return "POST";
    case RequestMethod::GET:
      return "GET";
    case RequestMethod::PUT:
      return "PUT";
    case RequestMethod::DELETE:
      return "DELETE";
    case RequestMethod::PATCH:
      return "PATCH";
    case RequestMethod::OPTIONS:
      return "OPTIONS";
    case RequestMethod::HEAD:
      return "HEAD";
    default:
      return "";
  }
}

inline std::string responseStatusToString(ResponseStatus status) {
  switch (status) {
    case ResponseStatus::OK:
      return "200 OK";
    case ResponseStatus::Created:
      return "201 Created";
    case ResponseStatus::Accepted:
      return "202 Accepted";
    case ResponseStatus::NoContent:
      return "204 No Content";
    case ResponseStatus::BadRequest:
      return "400 Bad Request";
    case ResponseStatus::Unauthorized:
      return "401 Unauthorized";
    case ResponseStatus::Forbidden:
      return "403 Forbidden";
    case ResponseStatus::NotFound:
      return "404 Not Found";
    case ResponseStatus::InternalServerError:
      return "500 Internal Server Error";
    default:
      return "";
  }
}
class Http {
 public:
  Http(std::unordered_map<std::string, std::string> headers, std::string body)
      : _headers(std::move(headers)), _body(std::move(body)) {}

  std::unordered_map<std::string, std::string> _headers;
  std::string _body;
};

class HttpRequest : public Http {
 public:
  HttpRequest(RequestMethod method, const std::string& path,
              const std::unordered_map<std::string, std::string>& headers,
              const std::string& body)
      : _method(method),
        _path(path),
        Http(std::move(headers), std::move(body)) {}

  RequestMethod _method;
  std::string _path;
};

class HttpResponse : public Http {
 public:
  HttpResponse(int statusCode,
               const std::unordered_map<std::string, std::string>& headers,
               const std::string& body)
      : _statusCode(statusCode), Http(std::move(headers), std::move(body)) {}

  int _statusCode;
};
}  // namespace qabot::http