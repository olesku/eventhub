#include <openssl/sha.h>
#include <string.h>

#include "Common.hpp"
#include "Config.hpp"
#include "ConnectionWorker.hpp"
#include "TopicManager.hpp"
#include "Util.hpp"
#include "http/Handler.hpp"
#include "http/RequestStateMachine.hpp"
#include "http/Response.hpp"

using namespace std;

namespace eventhub {
namespace http {
void Handler::process(ConnectionPtr conn, const char* buf, size_t nBytes) {
  auto& req = conn->getHttpRequest();

  switch (req->process(buf, nBytes)) {
    case State::REQ_INCOMPLETE:
      return;
      break;

    case State::REQ_OK:
      _handlePath(conn);
      break;

    default:
      conn->shutdown();
  }
}

void Handler::_handlePath(ConnectionPtr conn) {
  auto& req = conn->getHttpRequest();

  if (req->getPath().compare("/status") == 0) {
    // TODO: implement status endpoint.
    conn->shutdown();
    return;
  }

  // Check authorization.
  std::string authToken;

  if (!req->getHeader("authorization").empty()) {
    authToken = Util::uriDecode(req->getHeader("authorization"));
  } else if (!req->getQueryString("auth").empty()) {
    authToken = Util::uriDecode(req->getQueryString("auth"));
  } else {
    _badRequest(conn, "No authentication token was given.", 401);
    return;
  }

  if (!conn->getAccessController().authenticate(authToken, Config.getJWTSecret())) {
    _badRequest(conn, "Authentication failed.", 401);
    return;
  }

  _websocketHandshake(conn);
}

bool Handler::_websocketHandshake(ConnectionPtr conn) {
  auto& req           = conn->getHttpRequest();
  const auto secWsKey = req->getHeader("sec-websocket-key");

  if (req->getHeader("upgrade").compare("websocket") != 0 || secWsKey.empty()) {
    _badRequest(conn, "Invalid websocket request.");
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

  conn->write(resp.get());
  conn->setState(ConnectionState::WEBSOCKET);

  // We don't need the HTTP request object any more after transition to Websocket state.
  conn->getHttpRequest().reset();

  return true;
}

void Handler::_badRequest(ConnectionPtr conn, const std::string reason, int statusCode) {
  Response resp;
  std::stringstream body;

  body << "<h1>" << statusCode << " " << resp.getStatusMsg(statusCode) << "</h1>\n";
  body << reason << "\r\n";

  resp.setStatus(statusCode);
  resp.setHeader("connection", "close");
  resp.setBody(body.str());
  conn->write(resp.get());
  conn->shutdown();
}

} // namespace http
} // namespace eventhub
