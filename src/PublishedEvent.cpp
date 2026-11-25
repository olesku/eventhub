#include "PublishedEvent.hpp"

namespace eventhub {

const nlohmann::json& PublishedEvent::json() const {
  std::call_once(_parse_once, [this] {
    _json = nlohmann::json::parse(_data, nullptr, false);
    std::string{}.swap(_data);
  });
  return _json;
}

const websocket::FanoutMessage& PublishedEvent::websocketMessage() const {
  std::call_once(_websocket_once, [this] { _websocket_message.emplace(json()); });
  return *_websocket_message;
}

} // namespace eventhub
