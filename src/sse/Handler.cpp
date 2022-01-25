#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include <initializer_list>

#include "Config.hpp"
#include "Connection.hpp"
#include "HandlerContext.hpp"
#include "Redis.hpp"
#include "Server.hpp"
#include "TopicManager.hpp"
#include "Util.hpp"
#include "sse/Handler.hpp"
#include "sse/Response.hpp"
#include "AccessController.hpp"
#include "http/Parser.hpp"
#include "jwt/json/json.hpp"

namespace eventhub {
namespace sse {

void Handler::HandleRequest(HandlerContext& ctx, http::Parser* req) {
  auto conn               = ctx.connection();
  auto& redis             = ctx.server()->getRedis();
  auto& accessController  = conn->getAccessController();

  auto path        = Util::uriDecode(req->getPath());
  auto lastEventId = req->getHeader("Last-Event-ID");
  auto sinceStr    = req->getQueryString("since");
  auto limitStr    = req->getQueryString("limit");
  long long limit  = ctx.server()->config().get<int>("max_cache_request_limit");

  if (path.at(0) == '/') {
    path = path.substr(1, std::string::npos);
  }

  if (path.empty() || !TopicManager::isValidTopicOrFilter(path)) {
    Response::error(conn, "Invalid topic requested.");
    return;
  }

  // Check authorization.
  if (!accessController.allowSubscribe(path)) {
    Response::error(conn, "Insufficient access.", 401);
    return;
  }

  // Get last-event-id.
  if (!req->getQueryString("lastEventId").empty()) {
    lastEventId = req->getQueryString("lastEventId");
  }

  // Parse limit parameter.
  if (!limitStr.empty()) {
    try {
      auto limitParam = std::stoull(limitStr, nullptr, 10);

      if (limitParam < (unsigned long long)ctx.server()->config().get<int>("max_cache_request_limit")) {
        limit = limitParam;
      }
    } catch (...) {}
  }

  Response::ok(conn);
  conn->setState(ConnectionState::SSE);
  conn->subscribe(path, 0);

  // Send cache if requested.
  nlohmann::json result;
  if (!lastEventId.empty()) {
    try {
      redis.getCacheSinceId(path, lastEventId, limit, TopicManager::isValidTopicFilter(path), result);
    } catch (...) {}
  } else if (!sinceStr.empty()) {
    try {
      auto since = std::stoull(sinceStr, nullptr, 10);
      redis.getCacheSince(path, since, limit, TopicManager::isValidTopicFilter(path), result);
    } catch (...) {}
  }

  for (const auto& cacheItem : result) {
    Response::sendEvent(conn, cacheItem["id"], cacheItem["message"]);
  }
}

} // namespace sse
} // namespace eventhub