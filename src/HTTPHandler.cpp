#include <openssl/sha.h>
#include <string.h>

#include "Common.hpp"
#include "ConnectionWorker.hpp"
#include "HTTPHandler.hpp"
#include "HTTPResponse.hpp"
#include "TopicManager.hpp"
#include "Util.hpp"

using namespace std;

namespace eventhub {
void HTTPHandler::parse(std::shared_ptr<Connection>& conn, Worker* wrk, const char* buf, size_t nBytes) {
  auto& req = conn->getHttpRequest();

  switch (req.parse(buf, nBytes)) {
    case HTTPRequest::HTTP_REQ_INCOMPLETE:
      return;
      break;

    case HTTPRequest::HTTP_REQ_OK:
      _handlePath(conn, wrk, req);
      break;

    default:
      conn->shutdown();
  }
}

void HTTPHandler::_handlePath(std::shared_ptr<Connection>& conn, Worker* wrk, HTTPRequest& req) {
  if (req.getPath().compare("/status") == 0) {
    // TODO: implement status endpoint.
    conn->shutdown();
    return;
  }

  _websocketHandshake(conn, req);
}

bool HTTPHandler::_websocketHandshake(std::shared_ptr<Connection>& conn, HTTPRequest& req) {
  const auto secWsKey = req.getHeader("sec-websocket-key");

  if (req.getHeader("upgrade").compare("websocket") != 0 || secWsKey.empty()) {
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

  HTTPResponse resp;
  resp.setStatus(101);
  resp.setHeader("upgrade", "websocket");
  resp.setHeader("connection", "upgrade");
  resp.setHeader("sec-websocket-accept", secWsAccept);

  if (!req.getHeader("Sec-WebSocket-Protocol").empty()) {
    resp.setHeader("Sec-WebSocket-Protocol", req.getHeader("Sec-WebSocket-Protocol"));
  }

  conn->write(resp.get());
  conn->setState(Connection::WEBSOCKET_MODE);
  return true;
}

void HTTPHandler::_badRequest(std::shared_ptr<Connection>& conn, const std::string reason) {
  HTTPResponse resp;
  resp.setStatus(400, "Bad request");
  resp.setHeader("connection", "close");
  resp.setBody("<h1>400 Bad request</h1>\n" + reason + "\r\n");
  conn->write(resp.get());
  conn->shutdown();
}
} // namespace eventhub
