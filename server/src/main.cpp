#include <chrono>
#include <coroutine>
#include <iostream>
#include <nlohmann/json.hpp>

#include "awaitable/awaitable.hpp"
#include "http/http_parse.hpp"
#include "http/http_serialize.hpp"
#include "scope_manager/scope_manager.hpp"
#include "socket/secure_socket.hpp"
#include "socket/socket.hpp"
#include "task/task.hpp"
#ifdef _WIN32
#include "socket/windows_socket_impl.hpp"
using SocketImpl = qabot::socket::WindowsSocketImpl;
#else
#include "socket/unix_socket_impl.hpp"
using SocketImpl = qabot::socket::UnixSocketImpl;
#endif

#define AI_SERVER_URL "generativelanguage.googleapis.com"
#define HTTPS_PORT 443

qabot::task::Task<void> handleClient(
    qabot::socket::Socket<SocketImpl>&& clientSocket) {
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
            return clientSocketPtr->receive(1024 * 8);
          });

      if (clientMessage.empty()) {
        // Client disconnected
        std::cout << "Client disconnected." << std::endl;
        break;
      }

      auto httpRequest = qabot::http::parseRequest(clientMessage);

      auto jsonMessage = nlohmann::json::parse(httpRequest.body);

      std::string modelName = jsonMessage["model_name"];
      std::string apiKey = jsonMessage["api_key"];
      std::string prompt = jsonMessage["prompt"];
      std::string message = jsonMessage["message"];

      std::string responseBody;
      try {
        bool isChunked = false;
        std::string path = "/v1beta/models/" + modelName +
                           ":streamGenerateContent?alt=sse&key=" + apiKey;
        std::string fullUrl = "https://" + std::string(AI_SERVER_URL) +
                              path;  // For serializeRequest

        nlohmann::json requestJson = {
            {"contents",
             nlohmann::json::array(
                 {{{"role", "user"},
                   {"parts", nlohmann::json::array({{{"text", prompt}}})}},
                  {{"role", "user"},  // 假設 message 也是來自使用者
                   {"parts", nlohmann::json::array({{{"text", message}}})}}})}};

        auto request = qabot::http::serializeRequest(
            qabot::http::RequestMethod::POST,
            fullUrl,  // Pass only the path part to serializeRequest
            {
                {"Content-Type",
                 contentTypeToString(qabot::http::ContentType::JSON)},
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
            break;  // End of headers
          }  // end of header line

          // Parse the header line

          auto colonPos = headerLine.find(':');
          if (colonPos != std::string::npos) {
            std::string headerName = headerLine.substr(0, colonPos);
            std::string headerValue =
                headerLine.substr(colonPos + 1);  // Skip the colon
            // Trim leading whitespace from headerValue
            headerValue.erase(0, headerValue.find_first_not_of(" \t"));
            // Store or process the header as needed

            if (headerName == "Transfer-Encoding" && headerValue == "chunked") {
              isChunked = true;
            }
          }
        }  // End of headers

        // 3. Read the response body
        std::string responseBody;
        if (isChunked) {
          // If the response is chunked, we need to send initial headers
          // to the client
          std::stringstream initialHeadersSs;
          initialHeadersSs << "HTTP/1.1 200 OK\r\n";
          initialHeadersSs << "Content-Type: "
                           << contentTypeToString(
                                  qabot::http::ContentType::JSON)
                           << "\r\n";
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
              break;  // End of chunks
            }
            // convert hex to int
            int chunkSize = std::stoi(chunkSizeLine, nullptr, 16);
            // read the chunk data
            int totalBytesRead = 0;

            std::string chunkData;

            while (totalBytesRead < chunkSize) {  // Read the chunk data
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
      } catch (const std::exception& e) {
        std::cerr << "Error connecting to server: " << e.what() << std::endl;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error handling client: " << e.what() << std::endl;
  }
}

qabot::task::Task<void> serverAcceptLoop(
    qabot::socket::Socket<SocketImpl>& serverSocket) {
  std::cout << "Start listening\n";

  while (true) {
    try {
      auto client = std::move(co_await qabot::awaitable::Awaitable<
                              qabot::socket::Socket<SocketImpl>>(
          [&serverSocket]() -> qabot::socket::Socket<SocketImpl> {
            return serverSocket.accept();
          }));

      auto clientTask = handleClient(std::move(client));
      // move clientTask into scopeManager
      // so it won't be destructed when the function returns
      // or goes to next loop
      qabot::scope_manager::ScopeManager::getInstance()
          << std::move(clientTask);
    } catch (const std::exception& e) {
      std::cerr << "Error accepting connection: " << e.what() << std::endl;
      continue;
    }
  }
}

int main() {
  qabot::socket::Socket<SocketImpl> listeningSocket(
      qabot::socket::TransportProtocol::TCP, qabot::socket::IPVersion::IPv4);

  listeningSocket.bind("0.0.0.0", 38763);

  listeningSocket.listen(5);
  // Start the server accept loop

  try {
    auto serverTask = serverAcceptLoop(listeningSocket);
    while (true) {
      // Keep the main thread alive to allow coroutines to run
      std::this_thread::sleep_for(std::chrono::seconds(5));

      qabot::scope_manager::ScopeManager::getInstance().cleanUpTask();
    };
  } catch (const std::exception& e) {
    std::cerr << "Error in server accept loop: " << e.what() << std::endl;
  }
}
