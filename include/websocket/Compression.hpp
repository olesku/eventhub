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

// Inflated size allowed for a received message of compressedSize wire bytes.
// Bounding the expansion ratio keeps the work a client can demand roughly
// proportional to the bytes it sends; the floor leaves small messages alone.
constexpr std::size_t minInflatedSizeLimit = 64 * 1024;
constexpr std::size_t maxInflationRatio    = 64;

constexpr std::size_t inflatedSizeLimit(std::size_t compressedSize, std::size_t maxMessageSize) {
  // Dividing instead of multiplying avoids overflow for huge sizes.
  if (compressedSize > maxMessageSize / maxInflationRatio) {
    return maxMessageSize;
  }

  const auto scaled  = compressedSize * maxInflationRatio;
  const auto allowed = scaled < minInflatedSizeLimit ? minInflatedSizeLimit : scaled;

  return allowed < maxMessageSize ? allowed : maxMessageSize;
}

// Add byte-aligned, non-final DEFLATE blocks containing literal prefix bytes.
// The result can be followed by an independently compressed message suffix.
std::string encodeStoredPrefix(std::string_view prefix);
std::size_t storedPrefixOverhead(std::size_t size);

} // namespace eventhub::websocket
