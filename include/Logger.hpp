#ifndef INCLUDE_LOGGER_HPP_
#define INCLUDE_LOGGER_HPP_

#include <memory>
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

    static std::shared_ptr<spdlog::logger> getInstance() {
      static Logger instance;
      return instance._logger;
    }

   private:
    std::shared_ptr<spdlog::logger> _logger;
};

#define LOG Logger::getInstance()

}
#endif // INCLUDE_LOGGER_HPP_