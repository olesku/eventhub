#include <string.h>

#include "Common.hpp"
#include "Config.hpp"
#include "ConnectionWorker.hpp"
#include "TopicManager.hpp"
#include "Util.hpp"
#include "http/Handler.hpp"
#include "http/Parser.hpp"
#include "http/Response.hpp"
#include "HandlerContext.hpp"

using namespace std;

namespace eventhub {
namespace http {
void Handler::HandleRequest(HandlerContext&& ctx, Parser* req, RequestState reqState) {
  switch (reqState) {
    case RequestState::REQ_INCOMPLETE: return;

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

void Handler::_handlePath(HandlerContext &ctx, Parser* req) {
  if (req->getPath().compare("/status") == 0) {
    // TODO: implement status endpoint.
    ctx.connection()->shutdown();
    return;
  }

  // Check authorization.
  std::string authToken;

  if (!req->getHeader("authorization").empty()) {
    authToken = Util::uriDecode(req->getHeader("authorization"));
  } else if (!req->getQueryString("auth").empty()) {
    authToken = Util::uriDecode(req->getQueryString("auth"));
  } else {
    _badRequest(ctx, "No authentication token was given.", 401);
    return;
  }

  if (!ctx.connection()->getAccessController().authenticate(authToken, Config.getJWTSecret())) {
    _badRequest(ctx, "Authentication failed.", 401);
    return;
  }

  _websocketHandshake(ctx, req);
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
  ctx.connection()->shutdown();
}

} // namespace http
} // namespace eventhub
