#pragma once
#include <fstream>
#include <unordered_map>

namespace qabot::env_reader {
class EnvReader {
public:
  // singleton
  static EnvReader &getInstance() {
    static EnvReader instance;
    return instance;
  }

  void readEnv(const std::string &filePath) {
    std::ifstream envFile(filePath);
    if (!envFile.is_open()) {
      throw std::runtime_error("Failed to open env file");
    }
    std::string line;

    while (std::getline(envFile, line)) {
      auto delimiterPos = line.find('=');
      if (delimiterPos != std::string::npos) {
        std::string key = line.substr(0, delimiterPos);
        std::string value = line.substr(delimiterPos + 1);
        _envMap[key] = value;
      }
    }
  }

  std::string getEnv(const std::string &key) {
    auto it = _envMap.find(key);
    if (it != _envMap.end()) {
      return it->second;
    }
    return "";
  }

  // 禁止複製和移動
  EnvReader(const EnvReader &) = delete;
  EnvReader &operator=(const EnvReader &) = delete;

private:
  EnvReader() = default;
  ~EnvReader() = default;

  std::unordered_map<std::string, std::string> _envMap;
};
} // namespace qabot::env_reader