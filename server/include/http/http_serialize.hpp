#pragma once
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "http.hpp"

namespace qabot::http {
std::string serializeRequest(
    const RequestMethod method, const std::string& url,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& body);

std::string serializeResponse(
    const ResponseStatus statusCode,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& body);
}  // namespace qabot::http