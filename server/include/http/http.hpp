#pragma once
#include <nlohmann/json.hpp>

#include "socket/socket.hpp"

namespace qabot::http {
enum class RequestMethod {
  Post,
  Get,
  Put,
  Delete,
  Patch,
  Options,
  Head,
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
  Json,
  Html,
  Text,
  Xml,
};

inline std::string contentTypeToString(ContentType type) {
  switch (type) {
  case ContentType::Json:
    return "application/json";
  case ContentType::Html:
    return "text/html";
  case ContentType::Text:
    return "text/plain";
  case ContentType::Xml:
    return "application/xml";
  default:
    return "application/octet-stream";
  }
}

inline std::string requestMethodToString(RequestMethod method) {
  switch (method) {
  case RequestMethod::Post:
    return "POST";
  case RequestMethod::Get:
    return "GET";
  case RequestMethod::Put:
    return "PUT";
  case RequestMethod::Delete:
    return "DELETE";
  case RequestMethod::Patch:
    return "PATCH";
  case RequestMethod::Options:
    return "OPTIONS";
  case RequestMethod::Head:
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
struct Http {
  Http(const std::unordered_map<std::string, std::string> &headers)
      : headers(std::move(headers)) {}

  std::unordered_map<std::string, std::string> headers;
};

struct HttpRequest : public Http {
  HttpRequest(RequestMethod method, const std::string &path,
              const std::unordered_map<std::string, std::string> &headers,
              const std::string &body)
      : method(method), path(path), body(body), Http(std::move(headers)) {}

  RequestMethod method;
  std::string path;
  std::string body;
};

struct HttpResponse : public Http {
  HttpResponse(int statusCode,
               const std::unordered_map<std::string, std::string> &headers,
               const std::string &body)
      : statusCode(statusCode), body(body), Http(std::move(headers)) {}

  int statusCode;
  std::string body;
};
} // namespace qabot::http