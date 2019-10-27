#include <openssl/sha.h>
#include <string.h>

#include "common.hpp"
#include "connection_worker.hpp"
#include "http_handler.hpp"
#include "http_response.hpp"
#include "topic_manager.hpp"
#include "util.hpp"

using namespace std;

namespace eventhub {
void HTTPHandler::parse(std::shared_ptr<Connection>& conn, Worker* wrk, const char* buf, size_t n_bytes) {
  auto& req = conn->get_http_request();

  switch (req.parse(buf, n_bytes)) {
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
  if (req.get_path().compare("/status") == 0) {
    // TODO: implement status endpoint.
    conn->shutdown();
    return;
  }

  _websocketHandshake(conn, req);
}

bool HTTPHandler::_websocketHandshake(std::shared_ptr<Connection>& conn, HTTPRequest& req) {
  const auto sec_ws_key = req.get_header("sec-websocket-key");

  if (req.get_header("upgrade").compare("websocket") != 0 || sec_ws_key.empty()) {
    _badRequest(conn, "Invalid websocket request.");
    return false;
  }

  size_t key_len = sec_ws_key.length() + 36;
  unsigned char key[key_len];
  unsigned char key_sha1[SHA_DIGEST_LENGTH] = {0};

  memcpy(key, sec_ws_key.c_str(), sec_ws_key.length());
  memcpy(key + sec_ws_key.length(), WS_MAGIC_STRING, 36);

  SHA1(key, key_len, key_sha1);
  const std::string sec_ws_accept = Util::base64Encode(key_sha1, SHA_DIGEST_LENGTH);

  HTTPResponse resp;
  resp.SetStatus(101);
  resp.SetHeader("upgrade", "websocket");
  resp.SetHeader("connection", "upgrade");
  resp.SetHeader("sec-websocket-accept", sec_ws_accept);

  if (!req.get_header("Sec-WebSocket-Protocol").empty()) {
    resp.SetHeader("Sec-WebSocket-Protocol", req.get_header("Sec-WebSocket-Protocol"));
  }

  conn->write(resp.Get());
  conn->set_state(Connection::WEBSOCKET_MODE);
  return true;
}

void HTTPHandler::_badRequest(std::shared_ptr<Connection>& conn, const std::string reason) {
  HTTPResponse resp;
  resp.SetStatus(400, "Bad request");
  resp.SetHeader("connection", "close");
  resp.SetBody("<h1>400 Bad request</h1>\n" + reason + "\r\n");
  conn->write(resp.Get());
  conn->shutdown();
}
} // namespace eventhub
