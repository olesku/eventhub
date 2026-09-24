#include <string.h>
#include <openssl/sha.h>
#include <sstream>
#include <string>
#include <memory>

#include "Common.hpp"
#include "Config.hpp"
#include "HandlerContext.hpp"
#include "Server.hpp"
#include "Util.hpp"
#include "http/Handler.hpp"
#include "http/Request.hpp"
#include "http/Types.hpp"
#include "http/Response.hpp"
#include "metrics/JsonRenderer.hpp"
#include "metrics/PrometheusRenderer.hpp"
#include "sse/Handler.hpp"
#include "AccessController.hpp"
#include "Connection.hpp"

namespace eventhub {
namespace http {
void Handler::handleRequest(HandlerContext&& ctx, const Request& request) {
  _handlePath(ctx, request);
}

void Handler::handleError(HandlerContext&& ctx, ParseError error) {
  (void)error;
  ctx.connection()->shutdown();
}

void Handler::_handlePath(HandlerContext& ctx, const Request& request) {
  std::string method = request.method();
  Util::strToLower(method);

  // Only allow get and options requests.
  // Answer with CORS headers on options request.
  if (method == "options") {
    Response resp(204);
    _setCorsHeaders(request, resp);
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
  if (request.path() == "/healthz") {
    Response resp(200);
    _setCorsHeaders(request, resp);
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Connection", "close");
    resp.setBody("{ \"status\": \"ok\" }\r\n");

    ctx.connection()->write(resp.get());
    ctx.connection()->shutdownAfterFlush();
    return;
  }

  // Metrics endpoint.
  if (request.path() == "/metrics" || request.path() == "/metrics/") {
    Response resp(200);
    _setCorsHeaders(request, resp);
    resp.setHeader("Connection", "close");

    std::string m;
    if (request.queryParameter("format") == "json") {
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

  if (!request.header("authorization").empty()) {
    authToken = request.header("authorization");
  } else if (!request.queryParameter("auth").empty()) {
    authToken = Util::uriDecode(request.queryParameter("auth"));
  } else if (!ctx.server()->config().get<bool>("disable_auth")) {
    _badRequest(ctx, "No authentication token was given.", 401);
    return;
  }

  if (!ctx.connection()->getAccessController()->authenticate(authToken, ctx.server()->config().get<std::string>("jwt_secret"))) {
    _badRequest(ctx, "Authentication failed.", 401);
    return;
  }

  if (request.header("accept") == "text/event-stream") {
    if (!ctx.server()->config().get<bool>("enable_sse")) {
      _badRequest(ctx, "SSE is not enabled in this setup.", 501);
      return;
    }

    sse::Handler::handleRequest(ctx, request);
  } else if (request.header("upgrade") == "websocket") {
    _websocketHandshake(ctx, request);
  } else {
    _badRequest(ctx, "Invalid request.");
    return;
  }
}

bool Handler::_websocketHandshake(HandlerContext& ctx, const Request& request) {
  const auto secWsKey = request.header("sec-websocket-key");
  if (request.header("upgrade").compare("websocket") != 0 || secWsKey.empty()) {
    _badRequest(ctx, "Invalid websocket request.");
    return false;
  }

  const std::string key = secWsKey + WS_MAGIC_STRING;
  unsigned char keySha1[SHA_DIGEST_LENGTH] = {0};

  SHA1(reinterpret_cast<const unsigned char*>(key.c_str()), key.length(), keySha1);
  const std::string secWsAccept = Util::base64Encode(keySha1, SHA_DIGEST_LENGTH);

  Response resp;
  resp.setStatus(101);
  resp.setHeader("upgrade", "websocket");
  resp.setHeader("connection", "upgrade");
  resp.setHeader("sec-websocket-accept", secWsAccept);

  // FIXME: We should check against a list of supported protocols here.
  if (!request.header("Sec-WebSocket-Protocol").empty()) {
    resp.setHeader("Sec-WebSocket-Protocol", request.header("Sec-WebSocket-Protocol"));
  }

  ctx.connection()->write(resp.get());
  ctx.connection()->setState(ConnectionState::WEBSOCKET);

  return true;
}

void Handler::_badRequest(HandlerContext& ctx, const std::string& reason, int statusCode) {
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

void Handler::_setCorsHeaders(const Request& request, Response& resp) {
  const auto origin = request.header("Origin");

  if (origin.empty()) {
    resp.setHeader("Access-Control-Allow-Origin", "*");
  } else {
    resp.setHeader("Access-Control-Allow-Origin", origin);
  }
}

} // namespace http
} // namespace eventhub
