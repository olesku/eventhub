#include <string.h>
#include <openssl/sha.h>

#include "common.hpp"
#include "util.hpp"
#include "http_response.hpp"
#include "http_handler.hpp"

namespace eventhub {
  void http_handler::parse(std::shared_ptr<io::connection>& conn, const char* buf, size_t n_bytes) {
    auto& req = conn->get_http_request();

    switch(req.parse(buf, n_bytes)) {
      case http_request::HTTP_REQ_INCOMPLETE:
        return;
      break;

      case http_request::HTTP_REQ_OK:
        _handle_path(conn, req);
      break;

      default:
        conn->shutdown();
    }
  }

  void http_handler::_handle_path(std::shared_ptr<io::connection>& conn, http_request& req) {
    if (req.get_path().compare("/status") == 0) {
      // handle status;
      return;
    } 
  
    // TODO: check if path is in valid format.
    _websocket_handshake(conn, req);
  }

  bool http_handler::_websocket_handshake(std::shared_ptr<io::connection>& conn, http_request& req) {
    const auto sec_ws_key = req.get_header("sec-websocket-key");
    http_response resp;

    if (req.get_header("upgrade").compare("websocket") != 0 || sec_ws_key.empty()) {
      resp.SetStatus(400, "Bad request");
      resp.SetHeader("connection", "close");
      resp.SetBody("Bad request\r\n");
      conn->write(resp.Get());
      conn->shutdown();

      return false;
    }

    size_t key_len = sec_ws_key.length() + 36;
    unsigned char key[key_len];
    unsigned char key_sha1[SHA_DIGEST_LENGTH] = {0};

    memcpy(key, sec_ws_key.c_str(), sec_ws_key.length());
    memcpy(key+sec_ws_key.length(), WS_MAGIC_STRING, 36);

    SHA1(key, key_len, key_sha1);
    const std::string sec_ws_accept = util::base64_encode(key_sha1, SHA_DIGEST_LENGTH);

    resp.SetStatus(101);
    resp.SetHeader("upgrade", "websocket");
    resp.SetHeader("connection", "upgrade");
    resp.SetHeader("sec-websocket-accept", sec_ws_accept);

    if (!req.get_header("Sec-WebSocket-Protocol").empty()) {
      resp.SetHeader("Sec-WebSocket-Protocol", req.get_header("Sec-WebSocket-Protocol"));
    }

    conn->write(resp.Get());
    conn->set_state(io::connection::WEBSOCKET_MODE);
    return true;
  }
}
