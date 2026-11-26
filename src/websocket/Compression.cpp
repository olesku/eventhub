#include "websocket/Compression.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <zlib.h>

namespace eventhub::websocket {
namespace {

constexpr std::string_view trailer{"\x00\x00\xff\xff", 4};
constexpr std::size_t maxStoredBlockSize = 65535;
constexpr std::string_view response = "permessage-deflate; server_no_context_takeover; client_no_context_takeover";

std::string_view trim(std::string_view value) {
  const auto first = value.find_first_not_of(" \t");
  return first == value.npos ? std::string_view{} : value.substr(first, value.find_last_not_of(" \t") - first + 1);
}

std::string_view takePart(std::string_view& remaining, char separator, bool* more = nullptr) {
  bool quoted = false;
  for (std::size_t i = 0; i < remaining.size(); ++i) {
    if (remaining[i] == '"') {
      quoted = !quoted;
    } else if (quoted && remaining[i] == '\\' && i + 1 < remaining.size()) {
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
  const auto part = remaining;
  remaining = {};
  if (more) {
    *more = false;
  }
  return part;
}

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

struct Deflater {
  z_stream stream{};
  bool ready = deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK;

  ~Deflater() {
    if (ready) {
      deflateEnd(&stream);
    }
  }
};

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

std::string_view negotiateCompression(std::string_view offers) {
  while (!offers.empty()) {
    auto offer = trim(takePart(offers, ','));
    bool more;
    if (trim(takePart(offer, ';', &more)) != "permessage-deflate") {
      continue;
    }

    unsigned seen = 0;
    bool valid = true;
    while (more) {
      const auto parameter = takePart(offer, ';', &more);
      const auto equals = parameter.find('=');
      const auto name = trim(parameter.substr(0, equals));
      const bool hasValue = equals != parameter.npos;
      const auto value = hasValue ? trim(parameter.substr(equals + 1)) : std::string_view{};
      unsigned flag;
      if (name == "server_no_context_takeover" && !hasValue) {
        flag = 1;
      } else if (name == "client_no_context_takeover" && !hasValue) {
        flag = 2;
      } else if (name == "server_max_window_bits" && windowBits(value) == 15) {
        flag = 4;
      } else if (name == "client_max_window_bits" && (!hasValue || windowBits(value) != 0)) {
        flag = 8;
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
      return (seen & 4) ? "permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=15"
                        : response;
    }
  }
  return {};
}

std::string compressMessage(std::string_view message) {
  if (message.size() > std::numeric_limits<uInt>::max()) {
    return {};
  }
  thread_local Deflater deflater;
  auto& stream = deflater.stream;
  if (!deflater.ready || deflateReset(&stream) != Z_OK) {
    return {};
  }
  const auto bound = deflateBound(&stream, static_cast<uLong>(message.size())) + 6;
  if (bound > std::numeric_limits<uInt>::max()) {
    return {};
  }
  std::string output(bound, '\0');
  stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(message.data()));
  stream.avail_in = static_cast<uInt>(message.size());
  stream.next_out = reinterpret_cast<Bytef*>(output.data());
  stream.avail_out = static_cast<uInt>(output.size());
  if (deflate(&stream, Z_SYNC_FLUSH) != Z_OK || stream.avail_in != 0 || stream.avail_out == 0) {
    return {};
  }
  output.resize(output.size() - stream.avail_out);

  if (output.size() <= trailer.size() || output.compare(output.size() - trailer.size(), trailer.size(), trailer) != 0) {
    return {};
  }
  output.resize(output.size() - trailer.size());
  return output;
}

InflateResult decompressMessage(std::string_view message, std::size_t limit, std::string& output) {
  output.clear();
  if (message.empty() || message.size() > std::numeric_limits<uInt>::max()) {
    return InflateResult::INVALID;
  }
  thread_local Inflater inflater;
  auto& stream = inflater.stream;
  if (!inflater.ready || inflateReset(&stream) != Z_OK) {
    return InflateResult::INVALID;
  }
  stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(message.data()));
  stream.avail_in = static_cast<uInt>(message.size());
  std::array<char, 16384> buffer;
  do {
    stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
    stream.avail_out = buffer.size();
    const int result = inflate(&stream, Z_NO_FLUSH);
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
      return InflateResult::INVALID;
    }
  } while (stream.avail_in != 0 || stream.avail_out == 0);

  // The payload must end just before the LEN/NLEN fields of the empty
  // stored block. Don't let the synthetic trailer complete a truncated block.
  if (inflateSyncPoint(&stream) != 1) {
    return InflateResult::INVALID;
  }
  stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(trailer.data()));
  stream.avail_in = trailer.size();
  stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
  stream.avail_out = buffer.size();
  const int result = inflate(&stream, Z_SYNC_FLUSH);
  return (result == Z_OK || result == Z_STREAM_END) && stream.avail_in == 0 && stream.avail_out == buffer.size()
             ? InflateResult::OK
             : InflateResult::INVALID;
}

std::size_t storedPrefixOverhead(std::size_t size) {
  return 5 * (size / maxStoredBlockSize + (size % maxStoredBlockSize != 0));
}

std::string encodeStoredPrefix(std::string_view prefix) {
  std::string blocks;
  blocks.reserve(prefix.size() + storedPrefixOverhead(prefix.size()));
  while (!prefix.empty()) {
    const auto len = static_cast<std::uint16_t>(std::min(prefix.size(), maxStoredBlockSize));
    const auto nlen = static_cast<std::uint16_t>(~len);
    const char header[] = {0, static_cast<char>(len), static_cast<char>(len >> 8),
                           static_cast<char>(nlen), static_cast<char>(nlen >> 8)};
    blocks.append(header, sizeof(header));
    blocks.append(prefix.data(), len);
    prefix.remove_prefix(len);
  }
  return blocks;
}

} // namespace eventhub::websocket
