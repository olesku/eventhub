#include <functional>
#include <memory>
#include <string>
#include <vector>


#include "sse/Handler.hpp"
#include "http/Response.hpp"
#include "Common.hpp"
#include "Util.hpp"
//#include "Connection.hpp"
//#include "ConnectionWorker.hpp"
#include "HandlerContext.hpp"
#include "TopicManager.hpp"

namespace eventhub {
namespace sse {

void Handler::HandleRequest(HandlerContext& ctx, http::Parser* req) {
  auto path = Util::uriDecode(req->getPath());

  if (path.at(0) == '/') {
    path = path.substr(1, std::string::npos);
  }

  if (path.empty() || !TopicManager::isValidTopicOrFilter(path)) {
    LOG->info("Invalid SSE path: {}", path);
    ctx.connection()->shutdown();
  } else {
    http::Response resp(200, ":ok\n\n");
    resp.setHeader("Content-Type", "text/event-stream");
    ctx.connection()->write(resp.get());
    ctx.connection()->setState(ConnectionState::SSE);
    LOG->info("SSE client requested topic: {}", path);
  }
}

} // namespace SSE
} // namespace eventhub