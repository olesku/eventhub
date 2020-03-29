#ifndef INCLUDE_SSE_RESPONSE_HPP_
#define INCLUDE_SSE_RESPONSE_HPP_

#include "sse/Response.hpp"
#include "http/Response.hpp"

#include <memory>
#include <string>
#include <fmt/format.h>
//#include <nlohmann/json.hpp>

#include "Connection.hpp"

namespace eventhub {
namespace sse {
namespace response {

void ok(ConnectionPtr conn) {
  http::Response resp(200, ":ok\n\n");
  resp.setHeader("Access-Control-Allow-Origin", "*");
  resp.setHeader("Content-Type", "text/event-stream");
  conn->write(resp.get());
}

void sendPing(ConnectionPtr conn) {
  conn->write(":\n\n");
}

void sendEvent(ConnectionPtr conn, const std::string& id, const std::string& topic, const std::string& message, const std::string& event) {
  const auto data = fmt::format("id: {}\nevent: {}\ndata: {}\n\n", id, (event.empty() ? topic : event), message);
  conn->write(data);
}

void error(ConnectionPtr conn, const std::string& message, unsigned int statusCode) {
  nlohmann::json j;
  j["error"] = message;

  http::Response resp(statusCode, fmt::format("{}\n", j.dump()));
  conn->write(resp.get());
  conn->shutdown();
}

} // namespace response
} // namespace sse
} // namespace eventhub

#endif // INCLUDE_SSE_RESPONSE_HPP_
