#include <memory>
#include <openssl/sha.h>
#include <sstream>
#include <string.h>
#include <string>

#include "AccessController.hpp"
#include "Common.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "HandlerContext.hpp"
#include "Server.hpp"
#include "Util.hpp"
#include "http/Handler.hpp"
#include "http/Request.hpp"
#include "http/Response.hpp"
#include "http/Types.hpp"
#include "metrics/JsonRenderer.hpp"
#include "metrics/PrometheusRenderer.hpp"
#include "sse/Handler.hpp"

namespace eventhub {
namespace http {
void Handler::handleRequest(HandlerContext&& ctx, const Request& request) {
  _handlePath(ctx, request);
}

void Handler::handleError(HandlerContext&& ctx, ParseError error) {
  (void)error;
  ctx.connection()->close();
}

void Handler::_handlePath(HandlerContext& ctx, const Request& request) {
  std::string method = request.method();
  Util::strToLower(method);

  // Only allow get and options requests.
  // Answer with CORS headers on options request.
  if (method == "options") {
    Response response(204);
    _setCorsHeaders(request, response);
    response.setHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response.setHeader("Connection", "close");

    ctx.connection()->write(response.get());
    ctx.connection()->closeAfterFlush();
    return;
  } else if (method != "get") {
    Response response(405, "<h1>405 Method not allowed</h1>\r\n");
    response.setHeader("Connection", "close");

    ctx.connection()->write(response.get());
    ctx.connection()->closeAfterFlush();
    return;
  }

  // Healthcheck endpoint.
  if (request.path() == "/healthz") {
    Response response(200);
    _setCorsHeaders(request, response);
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Connection", "close");
    response.setBody("{ \"status\": \"ok\" }\r\n");

    ctx.connection()->write(response.get());
    ctx.connection()->closeAfterFlush();
    return;
  }

  // Metrics endpoint.
  if (request.path() == "/metrics" || request.path() == "/metrics/") {
    Response response(200);
    _setCorsHeaders(request, response);
    response.setHeader("Connection", "close");

    std::string m;
    if (request.queryParameter("format") == "json") {
      m = metrics::JsonRenderer::RenderMetrics(ctx.metricsSnapshot());
      response.setHeader("Content-Type", "application/json");
    } else {
      response.setHeader("Content-Type", "text/plain");
      m = metrics::PrometheusRenderer::RenderMetrics(ctx.metricsSnapshot(), ctx.config());
    }

    response.setBody(m);

    ctx.connection()->write(response.get());
    ctx.connection()->closeAfterFlush();
    return;
  }

  // Check authorization.
  std::string authToken;

  if (!request.header("authorization").empty()) {
    authToken = request.header("authorization");
  } else if (!request.queryParameter("auth").empty()) {
    authToken = Util::uriDecode(request.queryParameter("auth"));
  } else if (!ctx.config().get<bool>("disable_auth")) {
    _badRequest(ctx, "No authentication token was given.", 401);
    return;
  }

  if (!ctx.connection()->getAccessController()->authenticate(authToken, ctx.config().get<std::string>("jwt_secret"))) {
    _badRequest(ctx, "Authentication failed.", 401);
    return;
  }

  const bool requestsStreamingProtocol = request.header("accept") == "text/event-stream" ||
                                         request.header("upgrade") == "websocket";
  const auto contentLength = request.header("content-length");
  if (requestsStreamingProtocol &&
      (!request.header("transfer-encoding").empty() ||
       (!contentLength.empty() && contentLength != "0"))) {
    _badRequest(ctx, "Request bodies are not supported for protocol upgrades.");
    return;
  }

  if (request.header("accept") == "text/event-stream") {
    if (!ctx.config().get<bool>("enable_sse")) {
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

  const std::string key                    = secWsKey + WS_MAGIC_STRING;
  unsigned char keySha1[SHA_DIGEST_LENGTH] = {0};

  SHA1(reinterpret_cast<const unsigned char*>(key.c_str()), key.length(), keySha1);
  const std::string secWsAccept = Util::base64Encode(keySha1, SHA_DIGEST_LENGTH);

  Response response;
  response.setStatus(101);
  response.setHeader("upgrade", "websocket");
  response.setHeader("connection", "upgrade");
  response.setHeader("sec-websocket-accept", secWsAccept);

  // FIXME: We should check against a list of supported protocols here.
  if (!request.header("Sec-WebSocket-Protocol").empty()) {
    response.setHeader("Sec-WebSocket-Protocol", request.header("Sec-WebSocket-Protocol"));
  }

  if (!ctx.connection()->write(response.get()) || !ctx.connection()->upgradeToWebSocket()) {
    ctx.connection()->close();
    return false;
  }

  return true;
}

void Handler::_badRequest(HandlerContext& ctx, const std::string& reason, int statusCode) {
  Response response;
  std::stringstream body;

  body << "<h1>" << statusCode << " " << response.getStatusMsg(statusCode) << "</h1>\n";
  body << reason << "\r\n";

  response.setStatus(statusCode);
  response.setHeader("connection", "close");
  response.setBody(body.str());
  ctx.connection()->write(response.get());
  ctx.connection()->closeAfterFlush();
}

void Handler::_setCorsHeaders(const Request& request, Response& response) {
  const auto origin = request.header("Origin");

  if (origin.empty()) {
    response.setHeader("Access-Control-Allow-Origin", "*");
  } else {
    response.setHeader("Access-Control-Allow-Origin", origin);
  }
}

} // namespace http
} // namespace eventhub
