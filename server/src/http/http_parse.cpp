#include "http/http_parse.hpp"

namespace qabot::http {
HttpRequest parseRequest(const std::string &rawHttp) {
  std::stringstream requestStream(rawHttp);
  std::string line;

  // get first line of header
  std::getline(requestStream, line);
  std::stringstream lineStream(line);
  std::unordered_map<std::string, std::string> headers;
  std::string method, path, version;
  lineStream >> method >> path >> version;

  RequestMethod requestMethod;
  if (method == "POST") {
    requestMethod = RequestMethod::Post;
  } else if (method == "GET") {
    requestMethod = RequestMethod::Get;
  } else if (method == "PUT") {
    requestMethod = RequestMethod::Put;
  } else if (method == "DELETE") {
    requestMethod = RequestMethod::Delete;
  } else if (method == "PATCH") {
    requestMethod = RequestMethod::Patch;
  } else if (method == "OPTIONS") {
    requestMethod = RequestMethod::Options;
  } else if (method == "HEAD") {
    requestMethod = RequestMethod::Head;
  } else {
    throw std::runtime_error("Unsupported HTTP method");
  }

  // get each line of header
  while (getline(requestStream, line)) {
    std::stringstream lineStream(line);
    std::string key;

    std::string value;

    lineStream >> key >> value;
    if (line == "\r") {
      break; // End of headers
    }
    if (key.empty() || value.empty()) {
      break;
    }

    // remove ":" from key
    if (key.back() == ':') {
      key.pop_back();
    }
    // remove "\r" from value
    if (value.back() == '\r') {
      value.pop_back();
    }

    // store header
    headers[key] = value;

    if (key == "Content-Type:") {
      if (value != "application/json") {
        throw std::runtime_error("Unsupported Content Type");
      }
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

  return HttpRequest{requestMethod, path, headers, body};
}
HttpResponse parseResponse(const std::string &rawHttp) {
  std::stringstream responseStream(rawHttp);
  std::string line;

  // get first line of header
  std::getline(responseStream, line);

  std::stringstream lineStream(line);
  std::unordered_map<std::string, std::string> headers;
  std::string version;
  std::string statusCode;
  std::string statusMessage;

  lineStream >> version >> statusCode >> statusMessage;

  if (statusCode != "200") {
    throw std::runtime_error(statusCode);
  }

  // get each line of header
  while (getline(responseStream, line)) {
    std::stringstream lineStream(line);
    std::string key;
    std::string value;
    lineStream >> key >> value;
    // remove ":" from key
    if (key.back() == ':') {
      key.pop_back();
    }

    // remove "\r" from value
    if (value.back() == '\r') {
      value.pop_back();
    }

    // store header
    headers[key] = value;
    if (line == "\r") {
      break; // End of headers
    }
  }

  // get body content
  std::string body = responseStream.str().substr(responseStream.tellg());

  return HttpResponse{std::stoi(statusCode), headers,
                      body}; // Convert statusCode to int
}
} // namespace qabot::http