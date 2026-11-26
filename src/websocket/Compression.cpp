#include "websocket/Compression.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#define ZLIB_CONST
#include <zlib.h>

namespace eventhub::websocket {
namespace {

// A sync flush ends with an empty stored block, 00 00 ff ff. RFC 7692 removes
// those four bytes from each message on the wire, and the receiver puts them
// back before inflating.
constexpr std::string_view trailer{"\x00\x00\xff\xff", 4};

// Largest payload a single stored (uncompressed) DEFLATE block can carry.
constexpr std::size_t maxStoredBlockSize = 65535;

// What we answer a compatible offer with. Neither side keeps its compression
// window between messages, so every message can be inflated on its own. That
// is what lets one compressed event be shared by all its subscribers.
constexpr std::string_view response = "permessage-deflate; server_no_context_takeover; client_no_context_takeover";

// Parameters a client may put in a permessage-deflate offer, as bit flags so
// that a repeated parameter can be detected.
constexpr unsigned serverNoContextTakeover = 1;
constexpr unsigned clientNoContextTakeover = 2;
constexpr unsigned serverMaxWindowBits     = 4;
constexpr unsigned clientMaxWindowBits     = 8;

/**
 * Strip leading and trailing spaces and tabs.
 */
std::string_view trim(std::string_view value) {
  const auto first = value.find_first_not_of(" \t");

  return first == value.npos ? std::string_view{} : value.substr(first, value.find_last_not_of(" \t") - first + 1);
}

/**
 * Cut the text up to the next separator off the front of remaining.
 * A separator inside a quoted string does not count.
 * @param remaining Input; left holding whatever follows the separator.
 * @param separator Character that ends the part.
 * @param more Set to true if a separator was found, false if the input ended.
 * @return The part before the separator.
 */
std::string_view takePart(std::string_view& remaining, char separator, bool* more = nullptr) {
  bool quoted = false;

  for (std::size_t i = 0; i < remaining.size(); ++i) {
    if (remaining[i] == '"') {
      quoted = !quoted;
    } else if (quoted && remaining[i] == '\\' && i + 1 < remaining.size()) {
      // Skip the escaped character, so an escaped quote doesn't end the string.
      ++i;
    } else if (!quoted && remaining[i] == separator) {
      const auto part = remaining.substr(0, i);
      remaining.remove_prefix(i + 1);

      if (more) {
        *more = true;
      }
      return part;
    }
  }

  // No separator left: the rest of the input is the last part.
  const auto part = remaining;
  remaining       = {};

  if (more) {
    *more = false;
  }
  return part;
}

/**
 * Parse the value of a max_window_bits parameter.
 * Only the exact forms 8 to 15 are accepted, optionally quoted. Anything else,
 * such as "07" or "15x", is invalid.
 * @return The number of bits, or 0 if the value is invalid.
 */
int windowBits(std::string_view value) {
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    value = value.substr(1, value.size() - 2);
  }

  if (value == "8" || value == "9") {
    return value[0] - '0';
  }

  if (value.size() == 2 && value[0] == '1' && value[1] >= '0' && value[1] <= '5') {
    return 10 + value[1] - '0';
  }

  return 0;
}

// zlib state for compressing. Created once per thread and reset per message.
// The negative window size selects raw DEFLATE, without a zlib header.
struct Deflater {
  z_stream stream{};
  bool ready = deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK;

  ~Deflater() {
    if (ready) {
      deflateEnd(&stream);
    }
  }
};

// zlib state for decompressing. Created once per thread and reset per message.
struct Inflater {
  z_stream stream{};
  bool ready = inflateInit2(&stream, -15) == Z_OK;

  ~Inflater() {
    if (ready) {
      inflateEnd(&stream);
    }
  }
};

} // namespace

/**
 * Choose how to answer the Sec-WebSocket-Extensions header of a handshake.
 * The header is a comma separated list of offers in the client's order of
 * preference, each of them a name followed by parameters:
 *
 *   permessage-deflate; client_max_window_bits, permessage-deflate
 *
 * The first permessage-deflate offer we can honour wins. An offer with an
 * unknown, malformed or repeated parameter must be declined as a whole.
 * @param offers Value of the request header.
 * @return The value for the response header, or empty if nothing was accepted.
 */
std::string_view negotiateCompression(std::string_view offers) {
  while (!offers.empty()) {
    auto offer = trim(takePart(offers, ','));
    bool more;

    if (trim(takePart(offer, ';', &more)) != "permessage-deflate") {
      continue;
    }

    unsigned seen = 0;
    bool valid    = true;

    while (more) {
      const auto parameter = takePart(offer, ';', &more);
      const auto equals    = parameter.find('=');
      const auto name      = trim(parameter.substr(0, equals));
      const bool hasValue  = equals != parameter.npos;
      const auto value     = hasValue ? trim(parameter.substr(equals + 1)) : std::string_view{};
      unsigned flag;

      if (name == "server_no_context_takeover" && !hasValue) {
        flag = serverNoContextTakeover;
      } else if (name == "client_no_context_takeover" && !hasValue) {
        flag = clientNoContextTakeover;
      } else if (name == "server_max_window_bits" && windowBits(value) == 15) {
        // We always compress with a 15 bit window, so a smaller one can't be promised.
        flag = serverMaxWindowBits;
      } else if (name == "client_max_window_bits" && (!hasValue || windowBits(value) != 0)) {
        // The client says it can limit its window, or that it will. Either is
        // fine: we inflate with a 15 bit window, which covers every size.
        flag = clientMaxWindowBits;
      } else {
        valid = false;
        break;
      }

      if (seen & flag) {
        valid = false;
        break;
      }
      seen |= flag;
    }

    if (valid) {
      // A server_max_window_bits the client asked for has to be echoed back.
      return (seen & serverMaxWindowBits) ? "permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=15"
                                          : response;
    }
  }

  return {};
}

/**
 * Compress one message for a permessage-deflate frame.
 * @param message Uncompressed message.
 * @return Raw DEFLATE data with the trailer removed, or empty on failure.
 */
std::string compressMessage(std::string_view message) {
  // zlib counts input and output in uInt.
  if (message.size() > std::numeric_limits<uInt>::max()) {
    return {};
  }

  thread_local Deflater deflater;
  auto& stream = deflater.stream;

  // Resetting drops the window, as server_no_context_takeover requires.
  if (!deflater.ready || deflateReset(&stream) != Z_OK) {
    return {};
  }

  // deflateBound() is for a stream that is finished in one go. Leave room
  // for the sync flush marker as well.
  const auto bound = deflateBound(&stream, static_cast<uLong>(message.size())) + 6;
  if (bound > std::numeric_limits<uInt>::max()) {
    return {};
  }

  std::string output(bound, '\0');
  stream.next_in   = reinterpret_cast<const Bytef*>(message.data());
  stream.avail_in  = static_cast<uInt>(message.size());
  stream.next_out  = reinterpret_cast<Bytef*>(output.data());
  stream.avail_out = static_cast<uInt>(output.size());

  // With room for everything, one call has to consume all of the input and
  // leave space over. If not, the output may be incomplete.
  if (deflate(&stream, Z_SYNC_FLUSH) != Z_OK || stream.avail_in != 0 || stream.avail_out == 0) {
    return {};
  }
  output.resize(output.size() - stream.avail_out);

  // The data must end with the trailer, which is not sent.
  if (output.size() <= trailer.size() || output.compare(output.size() - trailer.size(), trailer.size(), trailer) != 0) {
    return {};
  }
  output.resize(output.size() - trailer.size());

  return output;
}

/**
 * Decompress the payload of a permessage-deflate message.
 * @param message Raw DEFLATE data as received, without the trailer.
 * @param limit Largest decompressed size to accept.
 * @param output Receives the decompressed message.
 * @return OK, TOO_BIG if the limit was exceeded, or INVALID for a bad payload.
 */
InflateResult decompressMessage(std::string_view message, std::size_t limit, std::string& output) {
  output.clear();

  if (message.empty() || message.size() > std::numeric_limits<uInt>::max()) {
    return InflateResult::INVALID;
  }

  thread_local Inflater inflater;
  auto& stream = inflater.stream;

  // Resetting drops the window, as client_no_context_takeover requires.
  if (!inflater.ready || inflateReset(&stream) != Z_OK) {
    return InflateResult::INVALID;
  }

  stream.next_in  = reinterpret_cast<const Bytef*>(message.data());
  stream.avail_in = static_cast<uInt>(message.size());
  std::array<char, 16384> buffer;

  // Inflate through a fixed buffer, so that the limit is checked before the
  // output grows rather than after.
  do {
    stream.next_out  = reinterpret_cast<Bytef*>(buffer.data());
    stream.avail_out = buffer.size();

    const int result    = inflate(&stream, Z_NO_FLUSH);
    const auto produced = buffer.size() - stream.avail_out;

    if (produced > limit - output.size()) {
      return InflateResult::TOO_BIG;
    }
    output.append(buffer.data(), produced);

    if (result == Z_STREAM_END) {
      // RFC 7692 permits BFINAL before the last stored block. Restart the
      // bitstream without copying or discarding this message's window.
      if (stream.avail_in == 0 || inflateResetKeep(&stream) != Z_OK) {
        return InflateResult::INVALID;
      }
    } else if (result != Z_OK && !(result == Z_BUF_ERROR && stream.avail_in == 0)) {
      // Z_BUF_ERROR with no input left only means that inflate is waiting for more.
      return InflateResult::INVALID;
    }
  } while (stream.avail_in != 0 || stream.avail_out == 0);

  // The payload must end just before the LEN/NLEN fields of the empty
  // stored block. Don't let the synthetic trailer complete a truncated block.
  if (inflateSyncPoint(&stream) != 1) {
    return InflateResult::INVALID;
  }

  // Feed the trailer the sender removed. It must be consumed in full and
  // must not produce any more data.
  stream.next_in   = reinterpret_cast<const Bytef*>(trailer.data());
  stream.avail_in  = trailer.size();
  stream.next_out  = reinterpret_cast<Bytef*>(buffer.data());
  stream.avail_out = buffer.size();

  const int result = inflate(&stream, Z_SYNC_FLUSH);

  return (result == Z_OK || result == Z_STREAM_END) && stream.avail_in == 0 && stream.avail_out == buffer.size()
             ? InflateResult::OK
             : InflateResult::INVALID;
}

/**
 * Number of bytes encodeStoredPrefix() adds to a prefix of the given size:
 * a five byte header for every stored block that is needed.
 */
std::size_t storedPrefixOverhead(std::size_t size) {
  return 5 * (size / maxStoredBlockSize + (size % maxStoredBlockSize != 0));
}

/**
 * Wrap bytes in stored DEFLATE blocks, so they can be put in front of data
 * that was compressed separately.
 * A stored block holds its bytes as they are and ends on a byte boundary, and
 * none of the blocks is marked as final. The result followed by an
 * independently compressed message is therefore one valid DEFLATE stream.
 * @param prefix Bytes to wrap.
 * @return The stored blocks.
 */
std::string encodeStoredPrefix(std::string_view prefix) {
  std::string blocks;
  blocks.reserve(prefix.size() + storedPrefixOverhead(prefix.size()));

  while (!prefix.empty()) {
    const auto len  = static_cast<std::uint16_t>(std::min(prefix.size(), maxStoredBlockSize));
    const auto nlen = static_cast<std::uint16_t>(~len);

    // Block header: BFINAL=0 and BTYPE=00 (stored), then the length and its
    // one's complement, both little-endian.
    const char header[] = {0, static_cast<char>(len), static_cast<char>(len >> 8),
                           static_cast<char>(nlen), static_cast<char>(nlen >> 8)};

    blocks.append(header, sizeof(header));
    blocks.append(prefix.data(), len);
    prefix.remove_prefix(len);
  }

  return blocks;
}

} // namespace eventhub::websocket
