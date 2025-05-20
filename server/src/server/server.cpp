#include "server/server.hpp"
#include <coroutine>
#include <memory>
#include <utility>

#include "awaitable/awaitable.hpp"
#include "env_reader/env_reader.hpp"
#include "http/http.hpp"
#include "http/http_parse.hpp"
#include "http/http_serialize.hpp"
#include "nlohmann/json.hpp"
#include "scope_manager/scope_manager.hpp"
#include "socket/secure_socket.hpp"
#include "socket/socket.hpp"
#include "socket/socket_exception.hpp"

#define AI_SERVER_URL "generativelanguage.googleapis.com"
#define HTTPS_PORT 443
namespace qabot::server {
void Server::start() {
  // Bind the socket to the address and port
  _serverSocket.bind("0.0.0.0", 38763);
  _serverSocket.listen(5);

  // Start the server loop
  auto serverTask = _serverLoop();
  // move serverTask into scopeManager
  qabot::scope_manager::ScopeManager::getInstance() << std::move(serverTask);
}

qabot::task::Task<void> Server::_serverLoop() {
  std::cout << "Start listening\n";

  while (true) {
    try {
      auto client = std::move(co_await qabot::awaitable::Awaitable<
                              qabot::socket::Socket<SocketImpl>>(
          [this]() -> qabot::socket::Socket<SocketImpl> {
            return _serverSocket.accept();
          }));

      auto clientTask = _clientLoop(std::move(client));
      // move clientTask into scopeManager
      // so it won't be destructed when the function returns
      // or goes to next loop
      qabot::scope_manager::ScopeManager::getInstance()
          << std::move(clientTask);
    } catch (const std::exception &e) {
      std::cerr << "Error accepting connection: " << e.what() << std::endl;
      continue;
    }
  }
}

qabot::task::Task<void>
Server::_clientLoop(qabot::socket::Socket<SocketImpl> &&clientSocket) {
  // Create a shared pointer to the client socket
  // This allows us to share the socket between the coroutine and the main
  // thread
  auto clientSocketPtr = std::make_shared<qabot::socket::Socket<SocketImpl>>(
      std::move(clientSocket));

  // Create a secure socket for sending data to the AI server
  auto sendingSocketPtr =
      std::make_shared<qabot::socket::SecureSocket<SocketImpl>>(
          qabot::socket::TransportProtocol::TCP,
          qabot::socket::IPVersion::IPv4);
  try {
    // connect to the AI server
    co_await qabot::awaitable::Awaitable<void>([sendingSocketPtr]() {
      sendingSocketPtr->connect(AI_SERVER_URL, HTTPS_PORT);
    });

    // Keep receiving messages from the client
    while (true) {
      auto clientMessage = co_await qabot::awaitable::Awaitable<std::string>(
          [clientSocketPtr]() -> std::string {
            return clientSocketPtr->receive(1024 * 1024 * 80);
          });

      if (clientMessage.empty()) {
        // Client disconnected
        std::cout << "Client disconnected." << std::endl;
        break;
      }

      std::cout << clientMessage << std::endl;

      auto httpRequest = qabot::http::parseRequest(clientMessage);

      auto jsonMessage = nlohmann::json::parse(httpRequest.body);

      std::string modelName = jsonMessage["model_name"];
      std::string apiKey =
          env_reader::EnvReader::getInstance().getEnv("API_KEY");
      std::string prompt = jsonMessage["prompt"];
      std::string message = jsonMessage["message"];

      bool isChunked = false;
      std::string path = "/v1beta/models/" + modelName +
                         ":streamGenerateContent?alt=sse&key=" + apiKey;
      std::string fullUrl = "https://" + std::string(AI_SERVER_URL) +
                            path; // For serializeRequest

      nlohmann::json contentArray = nlohmann::json::array();

      const auto &contextArray = jsonMessage["context"];

      for (const auto &context : contextArray) {
        for (auto const &[role, text] : context.items()) {
          contentArray.push_back({
              {"role", role},
              {
                  "parts",
                  {
                      {
                          {"text", text.get<std::string>()},
                      },
                  },
              },
          });
        }
      }

      // push back the last message from user
      contentArray.push_back({
          {"role", "user"},
          {
              "parts",
              {
                  {
                      {"text", message},
                  },
              },
          },
      });
      nlohmann::json requestJson = {
          {"system_instruction", {{"parts", {{{"text", prompt}}}}}},
          {"contents", contentArray}};

      auto request = qabot::http::serializeRequest(
          qabot::http::RequestMethod::Post,
          fullUrl, // Pass only the path part to serializeRequest
          {
              {"Content-Type",
               contentTypeToString(qabot::http::ContentType::Json)},
          },
          requestJson.dump());

      std::cout << "Request: " << request << std::endl;

      co_await qabot::awaitable::Awaitable<void>(
          [sendingSocketPtr, request]() { sendingSocketPtr->send(request); });

      // start parsing the header line by line
      while (true) {
        std::string headerLine;
        while (true) {
          auto nowChar = co_await qabot::awaitable::Awaitable<char>(
              [sendingSocketPtr]() -> char {
                char result = sendingSocketPtr->receive(1)[0];

                return result;
              });

          if (nowChar == '\n') {
            break;
          } else if (nowChar == '\r') {
            continue;
          } else {
            headerLine += nowChar;
          }
        }
        std::cout << headerLine << std::endl;
        if (headerLine.empty()) {
          break; // End of headers
        } // end of header line

        // Parse the header line

        auto colonPos = headerLine.find(':');
        if (colonPos != std::string::npos) {
          std::string headerName = headerLine.substr(0, colonPos);
          std::string headerValue =
              headerLine.substr(colonPos + 1); // Skip the colon
          // Trim leading whitespace from headerValue
          headerValue.erase(0, headerValue.find_first_not_of(" \t"));
          // Store or process the header as needed

          if (headerName == "Transfer-Encoding" && headerValue == "chunked") {
            isChunked = true;
          }
        } else {
          // first line of header
          std::stringstream lineStream(headerLine);
          std::string version;
          lineStream >> version;
          std::string statusCode;
          std::string statusMessage;
          lineStream >> statusCode >> statusMessage;

          if (statusCode != "200") {
            throw qabot::socket::SocketException(std::stoi(statusCode),
                                                 statusMessage);
          }
        }
      } // End of headers

      // 3. Read the response body
      std::string responseBody;
      if (isChunked) {
        // If the response is chunked, we need to send initial headers
        // to the client
        std::stringstream initialHeadersSs;
        initialHeadersSs << "HTTP/1.1 200 OK\r\n";
        initialHeadersSs << "Content-Type: "
                         << "text/event-stream\r\n";

        initialHeadersSs << "Transfer-Encoding: chunked\r\n";
        initialHeadersSs << "Connection: keep-alive\r\n";

        initialHeadersSs << "\r\n";
        std::string initialResponseHeaders = initialHeadersSs.str();

        co_await qabot::awaitable::Awaitable<void>(
            [clientSocketPtr, initialResponseHeaders]() {
              clientSocketPtr->send(initialResponseHeaders);
            });
        // read the chunk size
        // first read one line
        while (true) {
          // read the chunk size
          std::string chunkSizeLine;
          while (true) {
            auto nowChar = co_await qabot::awaitable::Awaitable<char>(
                [sendingSocketPtr]() -> char {
                  return sendingSocketPtr->receive(1)[0];
                });
            if (nowChar == '\n') {
              break;
            } else if (nowChar == '\r') {
              continue;
            } else {
              chunkSizeLine += nowChar;
            }
          }

          co_await qabot::awaitable::Awaitable<void>(
              [clientSocketPtr, chunkSizeLine]() {
                clientSocketPtr->send(chunkSizeLine + "\r\n");
              });

          if (chunkSizeLine == "0") {
            co_await qabot::awaitable::Awaitable<void>(
                [sendingSocketPtr]() { sendingSocketPtr->receive(2); });
            co_await qabot::awaitable::Awaitable<void>(
                [clientSocketPtr]() { clientSocketPtr->send("\r\n"); });
            break; // End of chunks
          }
          // convert hex to int
          int chunkSize = std::stoi(chunkSizeLine, nullptr, 16);
          // read the chunk data
          int totalBytesRead = 0;

          std::string chunkData;

          while (totalBytesRead < chunkSize) { // Read the chunk data
            auto chunkFragment =
                co_await qabot::awaitable::Awaitable<std::string>(
                    [sendingSocketPtr,
                     byteToRead = chunkSize - totalBytesRead]() {
                      return sendingSocketPtr->receive(byteToRead);
                    });
            totalBytesRead += chunkFragment.size();
            chunkData += chunkFragment;
          }

          // send the chunkData to client

          co_await qabot::awaitable::Awaitable<void>(
              [clientSocketPtr, chunkData]() {
                clientSocketPtr->send(chunkData + "\r\n");
              });

          responseBody += chunkData;

          // read the trailing CRLF
          co_await qabot::awaitable::Awaitable<void>(
              [sendingSocketPtr]() { sendingSocketPtr->receive(2); });
        }
      } else {
        // If not chunked, read the response body directly
        auto body = co_await qabot::awaitable::Awaitable<std::string>(
            [sendingSocketPtr]() -> std::string {
              return sendingSocketPtr->receive(1024 * 8);
            });

        responseBody = body;
      }
    }
  } catch (const qabot::socket::SocketException &e) {
    std::stringstream errorStream;
    errorStream << "HTTP/1.1 " << e.statusCode() << " " << e.what() << "\r\n";
    errorStream << "Content-Type: text/plain\r\n";
    errorStream << "Connection: close\r\n";
    errorStream << "\r\n";
    errorStream << "Error: " << e.what() << "\r\n";

    clientSocketPtr->send(errorStream.str());
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::stringstream errorStream;
    errorStream << "HTTP/1.1 500 Internal Server Error\r\n";
    errorStream << "Content-Type: text/plain\r\n";
    errorStream << "Connection: close\r\n";
    errorStream << "\r\n";
    errorStream << "Error: " << e.what() << "\r\n";

    clientSocketPtr->send(errorStream.str());
  }
}
} // namespace qabot::server