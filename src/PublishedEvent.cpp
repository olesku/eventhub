#include "PublishedEvent.hpp"

#include <string_view>

#include "websocket/Compression.hpp"

namespace eventhub {

/**
 * The publication as parsed JSON.
 * Parsed on first use and shared afterwards. The result is_discarded() if the
 * publication was not valid JSON.
 */
const nlohmann::json& PublishedEvent::json() const {
  std::call_once(_parse_once, [this] {
    _json = nlohmann::json::parse(_data, nullptr, false);

    // The raw text is not needed again; release it.
    std::string{}.swap(_data);
  });

  return _json;
}

/**
 * Everything of the JSON-RPC notification that follows the subscription ID.
 * A subscriber gets {"id":<its id> followed by this text, so the part that is
 * the same for all of them is built once.
 */
const std::string& PublishedEvent::websocketSuffix() const {
  std::call_once(_websocket_once, [this] {
    constexpr std::string_view prefix = R"(,"jsonrpc":"2.0","result":)";
    const auto dumped                 = json().dump();

    std::string suffix;
    suffix.reserve(prefix.size() + dumped.size() + 1);
    suffix.append(prefix);
    suffix.append(dumped);
    suffix.push_back('}');

    _websocket_suffix.swap(suffix);
  });

  return _websocket_suffix;
}

/**
 * websocketSuffix() compressed for permessage-deflate.
 * Compressed on first use and shared by every subscriber that negotiated
 * compression. Empty if compression failed or would not make it smaller.
 */
const std::string& PublishedEvent::compressedWebsocketSuffix() const {
  std::call_once(_compression_once, [this] {
    const auto& suffix = websocketSuffix();
    auto compressed    = websocket::compressMessage(suffix);

    if (compressed.size() < suffix.size()) {
      _compressed_suffix.swap(compressed);
    }
  });

  return _compressed_suffix;
}

} // namespace eventhub
