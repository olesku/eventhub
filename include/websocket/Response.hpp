#pragma once

#include <mutex>
#include <string>

#include "Connection.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {

class Response final {
public:
  static void sendData(ConnectionPtr conn, const std::string& data, FrameType frameType);
};

class FanoutMessage final {
public:
  explicit FanoutMessage(const nlohmann::json& result);
  void sendTo(Connection& conn, const jsonrpcpp::Id& id) const;

private:
  std::string _suffix;
  mutable std::once_flag _compression_once;
  mutable std::string _compressed_suffix;
};

} // namespace websocket
} // namespace eventhub

