#include <string.h>

#include <sstream>
#include <string>

#include "Common.hpp"
#include "Config.hpp"
#include "ConnectionWorker.hpp"
#include "HandlerContext.hpp"
#include "Server.hpp"
#include "TopicManager.hpp"
#include "Util.hpp"
#include "http/Handler.hpp"
#include "http/Parser.hpp"
#include "http/Response.hpp"
#include "metrics/JsonRenderer.hpp"
#include "metrics/PrometheusRenderer.hpp"
#include "sse/Handler.hpp"

namespace eventhub {
namespace http {
void Handler::HandleRequest(HandlerContext&& ctx, Parser* req, RequestState reqState) {
  switch (reqState) {
    case RequestState::REQ_INCOMPLETE:
      return;

    case RequestState::REQ_TO_BIG:
      ctx.connection()->shutdown();
      return;
      break;

    case RequestState::REQ_OK:
      _handlePath(ctx, req);
      break;

    default:
      ctx.connection()->shutdown();
  }
}

void Handler::_handlePath(HandlerContext& ctx, Parser* req) {
  std::string method = req->getMethod();
  Util::strToLower(method);

  // Only allow get and options requests.
  // Answer with CORS headers on options request.
  if (method == "options") {
    Response resp(204);
    _setCorsHeaders(req, resp);
    resp.setHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp.setHeader("Connection", "close");

    ctx.connection()->write(resp.get());
    ctx.connection()->shutdownAfterFlush();
    return;
  } else if (method != "get") {
    Response resp(405, "<h1>405 Method not allowed</h1>\r\n");
    resp.setHeader("Connection", "close");

    ctx.connection()->write(resp.get());
    ctx.connection()->shutdownAfterFlush();
    return;
  }

  // Healthcheck endpoint.
  if (req->getPath() == "/healthz") {
    Response resp(200);
    _setCorsHeaders(req, resp);
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Connection", "close");
    resp.setBody("{ \"status\": \"ok\" }\r\n");

    ctx.connection()->write(resp.get());
    ctx.connection()->shutdownAfterFlush();
    return;
  }

  // Metrics endpoint.
  if (req->getPath() == "/metrics" || req->getPath() == "/metrics/") {
    Response resp(200);
    _setCorsHeaders(req, resp);
    resp.setHeader("Connection", "close");

    std::string m;
    if (req->getQueryString("format") == "json") {
      m = metrics::JsonRenderer::RenderMetrics(ctx.server());
      resp.setHeader("Content-Type", "application/json");
    } else {
      resp.setHeader("Content-Type", "text/plain");
      m = metrics::PrometheusRenderer::RenderMetrics(ctx.server());
    }

    resp.setBody(m);

    ctx.connection()->write(resp.get());
    ctx.connection()->shutdownAfterFlush();
    return;
  }

  // Check authorization.
  std::string authToken;

  if (!req->getHeader("authorization").empty()) {
    authToken = req->getHeader("authorization");
  } else if (!req->getQueryString("auth").empty()) {
    authToken = Util::uriDecode(req->getQueryString("auth"));
  } else if (!ctx.server()->config().get<bool>("disable_auth")) {
    _badRequest(ctx, "No authentication token was given.", 401);
    return;
  }

  if (!ctx.connection()->getAccessController().authenticate(authToken, ctx.server()->config().get<std::string>("jwt_secret"))) {
    _badRequest(ctx, "Authentication failed.", 401);
    return;
  }

  if (req->getHeader("accept") == "text/event-stream") {
    if (!ctx.server()->config().get<bool>("enable_sse")) {
      _badRequest(ctx, "SSE is not enabled in this setup.", 501);
      return;
    }

    sse::Handler::HandleRequest(ctx, req);
  } else if (req->getHeader("upgrade") == "websocket") {
    _websocketHandshake(ctx, req);
  } else {
    _badRequest(ctx, "Invalid request.");
    return;
  }
}

bool Handler::_websocketHandshake(HandlerContext& ctx, Parser* req) {
  const auto secWsKey = req->getHeader("sec-websocket-key");
  if (req->getHeader("upgrade").compare("websocket") != 0 || secWsKey.empty()) {
    _badRequest(ctx, "Invalid websocket request.");
    return false;
  }

  size_t keyLen = secWsKey.length() + 36;
  unsigned char key[keyLen];
  unsigned char keySha1[SHA_DIGEST_LENGTH] = {0};

  memcpy(key, secWsKey.c_str(), secWsKey.length());
  memcpy(key + secWsKey.length(), WS_MAGIC_STRING, 36);

  SHA1(key, keyLen, keySha1);
  const std::string secWsAccept = Util::base64Encode(keySha1, SHA_DIGEST_LENGTH);

  Response resp;
  resp.setStatus(101);
  resp.setHeader("upgrade", "websocket");
  resp.setHeader("connection", "upgrade");
  resp.setHeader("sec-websocket-accept", secWsAccept);

  // FIXME: We should check against a list of supported protocols here.
  if (!req->getHeader("Sec-WebSocket-Protocol").empty()) {
    resp.setHeader("Sec-WebSocket-Protocol", req->getHeader("Sec-WebSocket-Protocol"));
  }

  ctx.connection()->write(resp.get());
  ctx.connection()->setState(ConnectionState::WEBSOCKET);

  return true;
}

void Handler::_badRequest(HandlerContext& ctx, const std::string reason, int statusCode) {
  Response resp;
  std::stringstream body;

  body << "<h1>" << statusCode << " " << resp.getStatusMsg(statusCode) << "</h1>\n";
  body << reason << "\r\n";

  resp.setStatus(statusCode);
  resp.setHeader("connection", "close");
  resp.setBody(body.str());
  ctx.connection()->write(resp.get());
  ctx.connection()->shutdownAfterFlush();
}

void Handler::_setCorsHeaders(Parser* req, Response& resp) {
  const auto& origin = req->getHeader("Origin");

  if (origin.empty()) {
    resp.setHeader("Access-Control-Allow-Origin", "*");
  } else {
    resp.setHeader("Access-Control-Allow-Origin", origin);
  }
}

} // namespace http
} // namespace eventhub
