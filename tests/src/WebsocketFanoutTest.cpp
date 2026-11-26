#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <zlib.h>

#include "catch.hpp"
#include "Config.hpp"
#include "ConnectionWorker.hpp"
#include "HandlerContext.hpp"
#include "PublishedEvent.hpp"
#include "Server.hpp"
#include "TopicManager.hpp"
#include "http/Handler.hpp"
#include "http/Parser.hpp"
#include "websocket/Compression.hpp"
#include "websocket/Handler.hpp"
#include "websocket/Response.hpp"

namespace {
std::atomic<std::size_t> compressionCount{0};
}

// The test executable redirects calls from eventhub_core here. Each message
// compression resets its thread-local deflater exactly once, regardless of size.
// These symbol names are required by the linker's --wrap option.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int __real_deflateReset(z_streamp stream);
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int __wrap_deflateReset(z_streamp stream) {
  compressionCount.fetch_add(1, std::memory_order_relaxed);
  return __real_deflateReset(stream);
}

namespace eventhub {
namespace {

using Type = ConfigValueType;
using Setting = ConfigValueSettings;
const ConfigMap websocketConfig = {
    {"redis_host", Type::STRING, "localhost", Setting::REQUIRED},
    {"redis_port", Type::INT, "6379", Setting::REQUIRED},
    {"redis_pool_size", Type::INT, "1", Setting::REQUIRED},
    {"redis_password", Type::STRING, "", Setting::OPTIONAL},
    {"redis_prefix", Type::STRING, "eventhub_test", Setting::OPTIONAL},
    {"disable_auth", Type::BOOL, "true", Setting::REQUIRED},
    {"jwt_secret", Type::STRING, "test", Setting::REQUIRED},
    {"enable_cache", Type::BOOL, "true", Setting::REQUIRED},
    {"enable_compression", Type::BOOL, "true", Setting::REQUIRED},
    {"default_cache_ttl", Type::INT, "60", Setting::REQUIRED},
    {"max_cache_request_limit", Type::INT, "100", Setting::REQUIRED},
};

class CapturedConnection final : public Connection {
public:
  using Connection::Connection;

  ssize_t flushSendBuffer() override {
    const auto size = _write_buffer.size();
    writes.emplace_back();
    writes.back().swap(_write_buffer);
    return size;
  }

  // Hand bytes to the connection as if they had been read from the socket.
  void receive(std::string_view wire) {
    _read_buffer.assign(wire.begin(), wire.end());
    _parseRequest(wire.size());
  }

  std::vector<std::string> writes;
};

// A client text frame. The mask is all zeroes, which leaves the payload as it is.
std::string clientTextFrame(std::string_view payload, bool compressed) {
  REQUIRE(payload.size() <= 65535);
  std::string frame(1, static_cast<char>(compressed ? 0xc1 : 0x81));
  if (payload.size() < 126) {
    frame.push_back(static_cast<char>(0x80 | payload.size()));
  } else {
    frame.push_back(static_cast<char>(0x80 | 126));
    frame.push_back(static_cast<char>(payload.size() >> 8));
    frame.push_back(static_cast<char>(payload.size()));
  }
  frame.append(4, '\0');
  frame.append(payload);
  return frame;
}

void upgrade(Config& cfg, Server& server, Worker& worker, const std::shared_ptr<CapturedConnection>& conn,
             const std::string& extensionHeaders, bool compressed) {
  http::Parser parser({[&](const http::Request& request) {
    http::Handler::handleRequest(HandlerContext(cfg, &server, &worker, conn), request);
  }, {}});
  const auto request = "GET / HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: upgrade\r\n"
                       "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n" +
                       extensionHeaders + "\r\n";
  parser.parse(request.data(), request.size());
  REQUIRE(conn->writes.size() == 1);
  CHECK(conn->writes.front().find("HTTP/1.1 101 ") == 0);
  if (compressed) {
    CHECK(conn->writes.front().find("Sec-WebSocket-Extensions: permessage-deflate; server_no_context_takeover; client_no_context_takeover\r\n") !=
          std::string::npos);
  } else {
    CHECK(conn->writes.front().find("Sec-WebSocket-Extensions:") == std::string::npos);
  }
  CHECK(conn->hasWebSocketCompression() == compressed);
  REQUIRE(conn->getState() == ConnectionState::WEBSOCKET);
  conn->writes.clear();
}

std::string_view textPayload(const std::string& frame, bool compressed) {
  REQUIRE(frame.size() >= 2);
  REQUIRE(static_cast<unsigned char>(frame[0]) == (compressed ? 0xc1 : 0x81));
  const auto tag = static_cast<unsigned char>(frame[1]);
  REQUIRE(tag <= 127);
  const std::size_t extension = tag < 126 ? 0 : (tag == 126 ? 2 : 8);
  REQUIRE(frame.size() >= 2 + extension);
  std::uint64_t size = extension ? 0 : tag;
  for (std::size_t i = 0; i < extension; ++i) {
    size = (size << 8) | static_cast<unsigned char>(frame[2 + i]);
  }
  REQUIRE(tag == (size < 126 ? size : (size <= 65535 ? 126 : 127)));
  REQUIRE(size == frame.size() - 2 - extension);
  return std::string_view(frame).substr(2 + extension);
}

} // namespace

TEST_CASE("WebSocket fanout shares compressed bytes across workers and topic matches", "[websocket][compression]") {
  Config cfg(websocketConfig);
  cfg.load();
  Server server(cfg);
  Worker first(&server, 1);
  Worker second(&server, 2);
  sockaddr_in address{};
  auto a = std::make_shared<CapturedConnection>(-1, &address, &first, cfg, ConnectionCallbacks{});
  auto b = std::make_shared<CapturedConnection>(-1, &address, &second, cfg, ConnectionCallbacks{});
  auto plain = std::make_shared<CapturedConnection>(-1, &address, &first, cfg, ConnectionCallbacks{});
  auto sse = std::make_shared<CapturedConnection>(-1, &address, &second, cfg, ConnectionCallbacks{});

  upgrade(cfg, server, first, a, "Sec-WebSocket-Extensions: permessage-deflate\r\n", true);
  upgrade(cfg, server, second, b, "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n", true);
  upgrade(cfg, server, first, plain, "", false);
  sse->setState(ConnectionState::SSE);

  const jsonrpcpp::Id idA(17);
  const jsonrpcpp::Id idB(std::string(70000, 'b') + "\"ö");
  const jsonrpcpp::Id idPlain("plain-id");
  a->subscribe("fanout/topic", idA);
  b->subscribe("fanout/+", idB);
  plain->subscribe("fanout/#", idPlain);
  sse->subscribe("fanout/topic", {});

  const std::string message(72000, 'x');
  const nlohmann::json result = {{"topic", "fanout/topic"}, {"id", "test-event"}, {"message", message}};
  const auto before = compressionCount.load();
  // Reading the plain representations must not trigger compression.
  const PublishedEvent plainEvent(result.dump());
  CHECK(plainEvent.json() == result);
  CHECK_FALSE(plainEvent.websocketSuffix().empty());
  CHECK(compressionCount.load() == before);

  const PublishedEvent event(result.dump());
  std::promise<void> start;
  auto ready = start.get_future();
  auto delivery = std::async(std::launch::async, [&] {
    ready.wait();
    first.getTopicManager()->publish("fanout/topic", event);
  });
  start.set_value();
  second.getTopicManager()->publish("fanout/topic", event);
  delivery.get();
  CHECK(compressionCount.load() == before + 1);

  REQUIRE(a->writes.size() == 1);
  REQUIRE(b->writes.size() == 1);
  REQUIRE(plain->writes.size() == 1);
  REQUIRE(sse->writes.size() == 1);
  const auto payloadA = textPayload(a->writes.front(), true);
  const auto payloadB = textPayload(b->writes.front(), true);
  const auto originalA = jsonrpcpp::Response(idA, result).to_json().dump();
  const auto originalB = jsonrpcpp::Response(idB, result).to_json().dump();
  std::string decoded;
  REQUIRE(websocket::decompressMessage(payloadA, originalA.size(), decoded) == websocket::InflateResult::OK);
  CHECK(decoded == originalA);
  REQUIRE(websocket::decompressMessage(payloadB, originalB.size(), decoded) == websocket::InflateResult::OK);
  CHECK(decoded == originalB);
  // The second recipient has a different ID spanning two stored blocks, but
  // the wire bytes after its prefix must match the first recipient exactly.
  CHECK(payloadA.substr(5 + 6 + idA.to_json().dump().size()) == payloadB.substr(10 + 6 + idB.to_json().dump().size()));
  CHECK(textPayload(plain->writes.front(), false) == jsonrpcpp::Response(idPlain, result).to_json().dump());
  CHECK(sse->writes.front() == "id: test-event\ndata: " + message + "\n\n");

  // Further matches reuse the cache; a new publication gets its own cache.
  first.getTopicManager()->publish("fanout/topic", event);
  second.getTopicManager()->publish("fanout/topic", event);
  CHECK(compressionCount.load() == before + 1);
  const PublishedEvent nextEvent(result.dump());
  first.getTopicManager()->publish("fanout/topic", nextEvent);
  second.getTopicManager()->publish("fanout/topic", nextEvent);
  CHECK(compressionCount.load() == before + 2);
}

TEST_CASE("A subscriber that cannot take an event does not stop fanout to the rest", "[websocket]") {
  Config cfg(websocketConfig);
  cfg.load();
  Server server(cfg);
  Worker worker(&server, 1);
  sockaddr_in address{};
  auto websocket = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});
  auto sse = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});
  upgrade(cfg, server, worker, websocket, "", false);
  sse->setState(ConnectionState::SSE);

  // New subscribers go first in the list, so the SSE one is served first.
  const jsonrpcpp::Id id(3);
  websocket->subscribe("fanout/partial", id);
  sse->subscribe("fanout/partial", {});

  // SSE needs a string "message"; WebSocket forwards the JSON as it is.
  const nlohmann::json result = {{"topic", "fanout/partial"}, {"id", "no-message"}};
  worker.getTopicManager()->publish("fanout/partial", PublishedEvent(result.dump()));

  CHECK(sse->writes.empty());
  REQUIRE(websocket->writes.size() == 1);
  CHECK(textPayload(websocket->writes.front(), false) == jsonrpcpp::Response(id, result).to_json().dump());
}

TEST_CASE("WebSocket parser errors close with the matching status code", "[websocket]") {
  Config cfg(websocketConfig);
  cfg.load();
  Server server(cfg);
  Worker worker(&server, 1);
  sockaddr_in address{};
  using websocket::ParserError;
  for (const auto& [error, code] : {std::pair{ParserError::INVALID_OPCODE, 1002}, std::pair{ParserError::INVALID_UTF8, 1007},
                                    std::pair{ParserError::INVALID_COMPRESSED_PAYLOAD, 1007},
                                    std::pair{ParserError::MESSAGE_TOO_BIG, 1009},
                                    std::pair{ParserError::INFLATION_LIMIT_EXCEEDED, 1009}}) {
    CAPTURE(code);
    auto conn = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});
    upgrade(cfg, server, worker, conn, "", false);
    websocket::Handler::handleError(HandlerContext(cfg, &server, &worker, conn), error);
    REQUIRE(conn->writes.size() == 1);
    CHECK(conn->writes.front() == std::string{static_cast<char>(0x88), 2, static_cast<char>(code >> 8), static_cast<char>(code & 0xff)});
  }
}

TEST_CASE("Metrics count WebSocket bytes on the wire and uncompressed", "[websocket][compression][metrics]") {
  Config cfg(websocketConfig);
  cfg.load();
  Server server(cfg);
  Worker worker(&server, 1);
  const auto& metrics = worker.getMetrics();
  sockaddr_in address{};
  auto compressed = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});
  auto plain = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});

  upgrade(cfg, server, worker, compressed, "Sec-WebSocket-Extensions: permessage-deflate\r\n", true);
  upgrade(cfg, server, worker, plain, "", false);
  auto sse = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});
  sse->setState(ConnectionState::SSE);
  CHECK(metrics.websocket_connections_count == 2);
  CHECK(metrics.compression_connections_count == 1);
  CHECK(metrics.websocket_sent_bytes == 0);

  // A published event: compressed for one subscriber, plain for the other.
  const jsonrpcpp::Id idCompressed(1);
  const jsonrpcpp::Id idPlain("plain");
  compressed->subscribe("metrics/topic", idCompressed);
  plain->subscribe("metrics/topic", idPlain);
  const nlohmann::json result = {{"topic", "metrics/topic"}, {"id", "event"}, {"message", std::string(4096, 'x')}};
  worker.getTopicManager()->publish("metrics/topic", PublishedEvent(result.dump()));
  REQUIRE(compressed->writes.size() == 1);
  REQUIRE(plain->writes.size() == 1);

  const auto originalCompressed = jsonrpcpp::Response(idCompressed, result).to_json().dump().size();
  const auto originalPlain = jsonrpcpp::Response(idPlain, result).to_json().dump().size();
  const auto wireCompressed = textPayload(compressed->writes.front(), true).size();
  REQUIRE(wireCompressed < originalCompressed);
  CHECK(textPayload(plain->writes.front(), false).size() == originalPlain);
  CHECK(metrics.websocket_sent_bytes == wireCompressed + originalPlain);
  CHECK(metrics.websocket_sent_uncompressed_bytes == originalCompressed + originalPlain);

  // An RPC reply that is compressed, and one that would not shrink.
  auto sentWire = metrics.websocket_sent_bytes.load();
  auto sentUncompressed = metrics.websocket_sent_uncompressed_bytes.load();
  const std::string reply(2000, 'y');
  websocket::Response::sendCompressibleText(*compressed, reply);
  websocket::Response::sendCompressibleText(*compressed, "x");
  REQUIRE(compressed->writes.size() == 3);
  CHECK(metrics.websocket_sent_bytes == sentWire + textPayload(compressed->writes[1], true).size() + 1);
  CHECK(metrics.websocket_sent_uncompressed_bytes == sentUncompressed + reply.size() + 1);

  // Control frames are not application data.
  sentWire = metrics.websocket_sent_bytes.load();
  websocket::Response::sendData(plain, "ping", websocket::FrameType::PING_FRAME);
  CHECK(metrics.websocket_sent_bytes == sentWire);

  // Received messages, with and without compression.
  const std::string text(1000, 'a');
  const auto deflated = websocket::compressMessage(text);
  REQUIRE(deflated.size() < text.size());
  compressed->receive(clientTextFrame(deflated, true));
  CHECK(metrics.websocket_received_bytes == deflated.size());
  CHECK(metrics.websocket_received_uncompressed_bytes == text.size());
  plain->receive(clientTextFrame("hello", false));
  CHECK(metrics.websocket_received_bytes == deflated.size() + 5);
  CHECK(metrics.websocket_received_uncompressed_bytes == text.size() + 5);

  // A message refused by the inflation limit.
  CHECK(metrics.inflation_limit_rejected_count == 0);
  websocket::Handler::handleError(HandlerContext(cfg, &server, &worker, plain), websocket::ParserError::MESSAGE_TOO_BIG);
  CHECK(metrics.inflation_limit_rejected_count == 0);
  websocket::Handler::handleError(HandlerContext(cfg, &server, &worker, compressed), websocket::ParserError::INFLATION_LIMIT_EXCEEDED);
  CHECK(metrics.inflation_limit_rejected_count == 1);

  // The gauges follow the connections.
  plain.reset();
  sse.reset();
  CHECK(metrics.websocket_connections_count == 1);
  CHECK(metrics.compression_connections_count == 1);
  compressed.reset();
  CHECK(metrics.websocket_connections_count == 0);
  CHECK(metrics.compression_connections_count == 0);
}

TEST_CASE("WebSocket eventlog compresses only when negotiated and beneficial", "[websocket][compression]") {
  Config cfg(websocketConfig);
  cfg.load();
  Server server(cfg);
  Worker worker(&server, 1);
  sockaddr_in address{};
  auto compressed = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});
  auto plain = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});

  upgrade(cfg, server, worker, compressed, "Sec-WebSocket-Extensions: permessage-deflate\r\n", true);
  upgrade(cfg, server, worker, plain, "", false);

  const std::string topic = "websocket/eventlog-compression";
  server.getRedis().connection().del({"eventhub_test:" + topic + ":cache", "eventhub_test:" + topic + ":scores"});
  const std::string message(4096, 'x');
  const auto eventId = server.getRedis().cacheMessage(topic, message, "");
  const std::string request = R"({"jsonrpc":"2.0","id":7,"method":"eventlog","params":{"topic":"websocket/eventlog-compression","since":1,"limit":1}})";
  for (const auto& conn : {compressed, plain}) {
    websocket::Handler::handleMessage(HandlerContext(cfg, &server, &worker, conn), websocket::FrameType::TEXT_FRAME, request);
    REQUIRE(conn->writes.size() == 1);
  }

  const auto payload = textPayload(compressed->writes.front(), true);
  const auto original = textPayload(plain->writes.front(), false);
  CHECK(payload.size() < original.size());
  std::string decoded;
  REQUIRE(websocket::decompressMessage(payload, original.size(), decoded) == websocket::InflateResult::OK);
  CHECK(decoded == original);
  const auto reply = nlohmann::json::parse(decoded);
  CHECK(reply.at("id") == 7);
  const auto& items = reply.at("result").at("items");
  REQUIRE(items.size() == 1);
  CHECK(items.at(0).at("id") == eventId);
  CHECK(items.at(0).at("message") == message);

  // Even on a negotiated connection, a payload that doesn't shrink stays plain.
  websocket::Response::sendCompressibleText(*compressed, "x");
  REQUIRE(compressed->writes.size() == 2);
  CHECK(textPayload(compressed->writes.back(), false) == "x");

  // Other RPC replies remain uncompressed.
  websocket::Handler::handleMessage(HandlerContext(cfg, &server, &worker, compressed), websocket::FrameType::TEXT_FRAME,
                                    R"({"jsonrpc":"2.0","id":"compressible-compressible-compressible-compressible","method":"ping"})");
  REQUIRE(compressed->writes.size() == 3);
  CHECK(nlohmann::json::parse(textPayload(compressed->writes.back(), false)).at("result").contains("pong"));
}

TEST_CASE("WebSocket compression is not negotiated when disabled in config", "[websocket][compression]") {
  ConfigMap disabledConfig = websocketConfig;
  for (auto& entry : disabledConfig) {
    if (entry.name == "enable_compression") {
      entry.defaultValue = "false";
    }
  }
  Config cfg(disabledConfig);
  cfg.load();
  Server server(cfg);
  Worker worker(&server, 1);
  sockaddr_in address{};
  auto conn = std::make_shared<CapturedConnection>(-1, &address, &worker, cfg, ConnectionCallbacks{});

  upgrade(cfg, server, worker, conn, "Sec-WebSocket-Extensions: permessage-deflate\r\n", false);
}

} // namespace eventhub
