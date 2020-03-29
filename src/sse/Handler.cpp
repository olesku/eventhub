#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "sse/Handler.hpp"
#include "sse/Response.hpp"
#include "http/Response.hpp"
#include "Common.hpp"
#include "Util.hpp"
#include "Server.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "HandlerContext.hpp"
#include "TopicManager.hpp"

namespace eventhub {
namespace sse {

void Handler::HandleRequest(HandlerContext& ctx, http::Parser* req) {
  auto conn = ctx.connection();
  auto& redis = ctx.server()->getRedis();
  auto& accessController = conn->getAccessController();

  auto path = Util::uriDecode(req->getPath());
  auto lastEventId = req->getQueryString("lastEventId");
  auto sinceStr = req->getQueryString("since");
  auto limitStr = req->getQueryString("limit");
  long long limit = Config.getInt("MAX_CACHE_REQUEST_LIMIT");

  if (path.at(0) == '/') {
    path = path.substr(1, std::string::npos);
  }

  if (path.empty() || !TopicManager::isValidTopicOrFilter(path)) {
    response::error(conn, "Invalid topic requested.");
    return;
  }

  // Check authorization.
  if (!accessController.allowSubscribe(path)) {
    response::error(conn, "Insufficient access.", 401);
    return;
  }

  // Parse limit parameter.
  if (!limitStr.empty()) {
    try {
      auto limitParam = std::stoull(limitStr, nullptr, 10);

      if (limitParam < (unsigned long long)Config.getInt("MAX_CACHE_REQUEST_LIMIT")) {
        limit = limitParam;
      }
    } catch(...) {}
  }

  response::ok(conn);
  conn->setState(ConnectionState::SSE);
  conn->subscribe(path, 0);

  // Send cache if requested.
  // TODO: Validate lastEventId before querying Redis.
  nlohmann::json result;
  if (!lastEventId.empty()) {
    try {
      redis.getCacheSinceId(path, lastEventId, limit, TopicManager::isValidTopicFilter(path), result);
    } catch(...) {}
  } else if (!sinceStr.empty()) {
    try {
      auto since = std::stoull(sinceStr, nullptr, 10);
      redis.getCacheSince(path, since, limit, TopicManager::isValidTopicFilter(path), result);
    } catch(...) {}
  }

  for (const auto& cacheItem : result) {
    response::sendEvent(conn, cacheItem["id"], cacheItem["topic"], cacheItem["message"], "");
  }
}

} // namespace SSE
} // namespace eventhub