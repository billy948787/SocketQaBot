#pragma once
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "http_types.hpp"

namespace qabot::http {
class HttpParser {
 public:
  HttpRequest parseRequest(const std::string& rawHttp) {
    std::stringstream requestStream(rawHttp);
    std::string line;
    std::cout << "start parsing\n";

    // get first line of header
    std::getline(requestStream, line);
    std::stringstream lineStream(line);
    std::string method, path, version;
    lineStream >> method >> path >> version;

    RequestMethod requestMethod;
    if (method == "POST") {
      requestMethod = RequestMethod::POST;
    } else if (method == "GET") {
      requestMethod = RequestMethod::GET;
    } else if (method == "PUT") {
      requestMethod = RequestMethod::PUT;
    } else if (method == "DELETE") {
      requestMethod = RequestMethod::DELETE;
    } else if (method == "PATCH") {
      requestMethod = RequestMethod::PATCH;
    } else if (method == "OPTIONS") {
      requestMethod = RequestMethod::OPTIONS;
    } else if (method == "HEAD") {
      requestMethod = RequestMethod::HEAD;
    } else {
      throw std::runtime_error("Unsupported HTTP method");
    }

    // get each line of header
    while (getline(requestStream, line)) {
      std::stringstream lineStream(line);
      std::string key;
      std::string value;

      lineStream >> key >> value;

      if (key == "Content-Type:") {
        if (value != "application/json") {
          throw std::runtime_error("Unsupported Content Type");
        }
      }
      if (line == "\r") {
        break;  // End of headers
      }
    }

    // get body content
    // str() returns the entire string
    // tellg() returns the current position of the stream
    // substr() returns the substring from the current position to the end
    std::string body = requestStream.str().substr(requestStream.tellg());

    if (body.empty()) {
      throw std::runtime_error("Empty body content");
    }

    return HttpRequest{.method = requestMethod, .path = path, .body = body};
  }

  HttpResponse parseResponse(const std::string& rawHttp) {
    std::stringstream responseStream(rawHttp);
    std::string line;

    // get first line of header
    std::getline(responseStream, line);

    std::stringstream lineStream(line);
    std::string version;
    std::string statusCode;
    std::string statusMessage;

    lineStream >> version >> statusCode >> statusMessage;

    if (statusCode != "200") {
      throw std::runtime_error(statusCode);
    }

    // get each line of header
    while (getline(responseStream, line)) {
      if (line == "\r") {
        break;  // End of headers
      }
    }

    // get body content
    std::string body = responseStream.str().substr(responseStream.tellg());

    return HttpResponse{.statusCode = std::stoi(statusCode),
                        .statusMessage = statusMessage,
                        .body = body};
  }
};
}  // namespace qabot::http