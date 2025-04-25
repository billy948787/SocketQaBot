#pragma once
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "http.hpp"

namespace qabot::http {
HttpRequest parseRequest(const std::string& rawHttp);
HttpResponse parseResponse(const std::string& rawHttp);
}  // namespace qabot::http