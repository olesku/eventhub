#ifndef INCLUDE_LOGGER_HPP_
#define INCLUDE_LOGGER_HPP_

#include <memory>
#include <string>
#include <iostream>
#include <map>
#include <stdlib.h>
//#include <spdlog/fwd.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "Config.hpp"

namespace eventhub {

class Logger {
  public:
    Logger() {
      _logger = spdlog::stdout_logger_mt("eventhub");
    }

    ~Logger() {}

    void setLevel(const std::string name) {
      std::map<std::string, spdlog::level::level_enum> levels = {
        { "info", spdlog::level::info },
        { "trace", spdlog::level::trace },
        { "debug", spdlog::level::debug },
        { "warning", spdlog::level::warn },
        { "error", spdlog::level::err },
        { "critical", spdlog::level::critical }
      };

      auto it = levels.find(name);
      if (it != levels.end()) {
        _logger->set_level(it->second);
      } else {
        std::cerr << "Error: Log level " << name << " is invalid." << std::endl;
        exit(1);
      }
    }

    std::shared_ptr<spdlog::logger> getLogger() {
      return _logger;
    }

    static Logger& getInstance() {
      static Logger _instance;
      return _instance;
    }

   private:
    std::shared_ptr<spdlog::logger> _logger;
};

#define LOG Logger::getInstance().getLogger()

}
#endif // INCLUDE_LOGGER_HPP_
