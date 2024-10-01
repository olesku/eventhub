#include <fmt/format.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <initializer_list>
#include <vector>

#include "sse/Response.hpp"
#include "http/Response.hpp"
#include "Connection.hpp"
#include "jwt/json/json.hpp"

namespace eventhub {
namespace sse {

void Response::ok(ConnectionPtr conn) {
  http::Response resp(200, ":ok\n\n");
  resp.setHeader("Access-Control-Allow-Headers", "Accept, Cache-Control, X-Requested-With, Last-Event-ID");
  resp.setHeader("Access-Control-Allow-Origin", "*");
  resp.setHeader("Cache-Control", "no-cache");
  resp.setHeader("Connection", "close");
  resp.setHeader("Content-Type", "text/event-stream");
  conn->write(resp.get());
}

void Response::sendPing(ConnectionPtr conn) {
  conn->write(":\n\n");
}

void Response::sendEvent(ConnectionPtr conn, const std::string& id, const std::string& message, const std::string& event) {
  std::string data;

  if (event.empty()) {
    data = fmt::format("id: {}\ndata: {}\n\n", id, message);
  } else {
    data = fmt::format("id: {}\nevent: {}\ndata: {}\n\n", id, event, message);
  }

  conn->write(data);
}

void Response::error(ConnectionPtr conn, std::string_view message, unsigned int statusCode) {
  nlohmann::json j;
  j["error"] = message;

  http::Response resp(statusCode, fmt::format("{}\n", j.dump()));
  conn->write(resp.get());
  conn->shutdownAfterFlush();
}

} // namespace sse
} // namespace eventhub
