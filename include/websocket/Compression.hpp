#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace eventhub::websocket {

// Returns an empty view if none of the offers supports our shared encoding.
std::string_view negotiateCompression(std::string_view offers);

// Raw DEFLATE with the permessage-deflate trailer removed. Empty on failure.
std::string compressMessage(std::string_view message);

enum class InflateResult { OK, INVALID, TOO_BIG };
InflateResult decompressMessage(std::string_view message, std::size_t limit, std::string& output);

// Add byte-aligned, non-final DEFLATE blocks containing literal prefix bytes.
// The result can be followed by an independently compressed message suffix.
std::string encodeStoredPrefix(std::string_view prefix);
std::size_t storedPrefixOverhead(std::size_t size);

} // namespace eventhub::websocket
