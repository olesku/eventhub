#pragma once

#include <mutex>
#include <string>

#include "jwt/json/json.hpp"

namespace eventhub {

// Shared by all workers and topic matches for one publication. Parse and
// encode only if there's a recipient for the corresponding representation.
class PublishedEvent final {
public:
  explicit PublishedEvent(const std::string& data) : _data(data) {}

  const nlohmann::json& json() const;
  const std::string& websocketSuffix() const;
  // Empty when compression failed or would not save space.
  const std::string& compressedWebsocketSuffix() const;

private:
  mutable std::string _data;
  mutable std::once_flag _parse_once;
  mutable nlohmann::json _json;
  mutable std::once_flag _websocket_once;
  mutable std::string _websocket_suffix;
  mutable std::once_flag _compression_once;
  mutable std::string _compressed_suffix;
};

} // namespace eventhub
