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
#define HTTPS_PORT 443  // --- OpenSSL Threading Setup (Example for < 1.1.0) ---
#if OPENSSL_VERSION_NUMBER < 0x10100000L
std::vector<std::mutex> openssl_mutexes;

void openssl_locking_callback(int mode, int n, const char* file, int line) {
  if (mode & CRYPTO_LOCK) {
    openssl_mutexes[n].lock();
  } else {
    openssl_mutexes[n].unlock();
  }
}

unsigned long openssl_id_callback() {
  // Simple thread ID, might need adjustment based on platform
  return static_cast<unsigned long>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

void setup_openssl_threading() {
  openssl_mutexes.resize(CRYPTO_num_locks());
  CRYPTO_set_id_callback(openssl_id_callback);
  CRYPTO_set_locking_callback(openssl_locking_callback);
  std::cerr << "OpenSSL threading callbacks set up (for OpenSSL < 1.1.0)."
            << std::endl;
}
#else
void setup_openssl_threading() {
  // OpenSSL 1.1.0+ handles threading internally
  std::cerr << "OpenSSL threading setup skipped (OpenSSL >= 1.1.0)."
            << std::endl;
}
#endif
// --- End OpenSSL Threading Setup ---

qabot::task::Task<std::string> receiveAndParseFromServer(
    const std::string& prompt, const std::string& message,
    const std::string& modelName, const std::string& apiKey,
    std::shared_ptr<qabot::socket::Socket<SocketImpl>> clientSocketPtr,
    std::shared_ptr<qabot::socket::SecureSocket<SocketImpl>> sendingSocketPtr) {
  // 使用 std::shared_ptr 確保 sendingSocket 在協程執行期間有效

  std::string responseBody;
  try {
    bool isChunked = false;
    std::string path = "/v1beta/models/" + modelName +
                       ":streamGenerateContent?alt=sse&key=" + apiKey;
    std::string fullUrl =
        "https://" + std::string(AI_SERVER_URL) + path;  // For serializeRequest

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
        auto receive_byte_awaitable = qabot::awaitable::Awaitable<std::string>(
            [sendingSocketPtr]() -> std::string {
              // Optional: Add logging inside the lambda if needed for further
              // debugging std::cerr << "[" << std::this_thread::get_id() << "]
              // receive(1) lambda: Calling receive(1)..." << std::endl;
              std::string result = sendingSocketPtr->receive(1);
              // std::cerr << "[" << std::this_thread::get_id() << "] receive(1)
              // lambda: receive(1) returned '" << (result.empty() ? "EMPTY" :
              // result) << "'" << std::endl;
              return result;
            });

        // Await the named variable
        auto receivedData = co_await receive_byte_awaitable;

        if (receivedData.empty()) {
          std::cerr << "Received empty data, connection may be closed."
                    << std::endl;
          co_return "";
        }
        char nowChar = receivedData[0];
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

        if (chunkSizeLine == "0") {
          co_await qabot::awaitable::Awaitable<void>(
              [sendingSocketPtr]() { sendingSocketPtr->receive(2); });
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

        // remove the "data: " in front of the chunk data
        if (chunkData.size() > 6 && chunkData.substr(0, 6) == "data: ") {
          chunkData = chunkData.substr(6);
        }

        // send the chunkData to client

        // co_await qabot::awaitable::Awaitable<void>(
        //     [clientSocketPtr, chunkData]() {
        //       clientSocketPtr->send(chunkData);
        //     });

        responseBody += chunkData;

        // read the trailing CRLF
        co_await qabot::awaitable::Awaitable<void>(
            [sendingSocketPtr]() { sendingSocketPtr->receive(2); });
      }

      co_return responseBody;

    } else {
      // If not chunked, read the response body directly
      auto body = co_await qabot::awaitable::Awaitable<std::string>(
          [sendingSocketPtr]() -> std::string {
            return sendingSocketPtr->receive(1024 * 8);
          });

      responseBody = body;

      co_return responseBody;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error connecting to server: " << e.what() << std::endl;
    co_return "";
  }
}

qabot::task::Task<void> handleClient(
    qabot::socket::Socket<SocketImpl>&& clientSocket) {
  // 使用 std::shared_ptr 確保 clientSocket 在協程執行期間有效
  auto clientSocketPtr = std::make_shared<qabot::socket::Socket<SocketImpl>>(
      std::move(clientSocket));

  // 使用 std::shared_ptr 確保 sendingSocket 在協程執行期間有效
  auto sendingSocketPtr =
      std::make_shared<qabot::socket::SecureSocket<SocketImpl>>(
          qabot::socket::TransportProtocol::TCP,
          qabot::socket::IPVersion::IPv4);
  try {
    // 1. Connect to the AI server
    co_await qabot::awaitable::Awaitable<void>([sendingSocketPtr]() {
      sendingSocketPtr->connect(AI_SERVER_URL, HTTPS_PORT);
    });
    // 或者確保 Task 能正確處理傳入物件的生命週期
    while (true) {  // 循環接收來自此客戶端的訊息
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

      auto response =
          co_await receiveAndParseFromServer(prompt, message, modelName, apiKey,
                                             clientSocketPtr, sendingSocketPtr);
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
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  setup_openssl_threading();
  qabot::socket::Socket<SocketImpl> listeningSocket(
      qabot::socket::TransportProtocol::TCP, qabot::socket::IPVersion::IPv4);

  listeningSocket.bind("localhost", 38763);

  listeningSocket.listen(5);
  // Start the server accept loop

  try {
    auto serverTask = serverAcceptLoop(listeningSocket);
    while (true);
  } catch (const std::exception& e) {
    std::cerr << "Error in server accept loop: " << e.what() << std::endl;
  }
}
