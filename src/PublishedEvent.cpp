#include "PublishedEvent.hpp"

#include "websocket/Compression.hpp"

namespace eventhub {

const nlohmann::json& PublishedEvent::json() const {
  std::call_once(_parse_once, [this] {
    _json = nlohmann::json::parse(_data, nullptr, false);
    std::string{}.swap(_data);
  });
  return _json;
}

const std::string& PublishedEvent::websocketSuffix() const {
  std::call_once(_websocket_once, [this] {
    auto suffix = json().dump();
    suffix.insert(0, R"(,"jsonrpc":"2.0","result":)");
    suffix += '}';
    _websocket_suffix.swap(suffix);
  });
  return _websocket_suffix;
}

const std::string& PublishedEvent::compressedWebsocketSuffix() const {
  std::call_once(_compression_once, [this] {
    const auto& suffix = websocketSuffix();
    auto compressed = websocket::compressMessage(suffix);
    if (compressed.size() < suffix.size()) {
      _compressed_suffix.swap(compressed);
    }
  });
  return _compressed_suffix;
}

} // namespace eventhub
