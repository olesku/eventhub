#include <string.h>
#include <openssl/sha.h>

#include "common.hpp"
#include "util.hpp"
#include "http_response.hpp"
#include "http_handler.hpp"
#include "topic_manager.hpp"

using namespace std;

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
    const string& path = req.get_path();

    if (path.empty() || path.compare("/") == 0 || path.at(0) != '/') {
      _bad_request(conn, "Nothing here!\r\n");
      return;
    }

    DLOG(INFO) << "Path: " << path;

    if (path.compare("/status") == 0) {
      // handle status;
      return;
    }
  
    const string topic_filter_name = path.substr(1, string::npos);
    if (!topic_manager::is_valid_topic_filter(topic_filter_name)) {
      _bad_request(conn, topic_filter_name + ": Topic name has invalid format.\r\n");
      return;
    }

    if (_websocket_handshake(conn, req)) {
      // Subscribe client to topic.
    }
  }

  bool http_handler::_websocket_handshake(std::shared_ptr<io::connection>& conn, http_request& req) {
    const auto sec_ws_key = req.get_header("sec-websocket-key");

    if (req.get_header("upgrade").compare("websocket") != 0 || sec_ws_key.empty()) {
      _bad_request(conn, "Invalid websocket request.");
      return false;
    }

    size_t key_len = sec_ws_key.length() + 36;
    unsigned char key[key_len];
    unsigned char key_sha1[SHA_DIGEST_LENGTH] = {0};

    memcpy(key, sec_ws_key.c_str(), sec_ws_key.length());
    memcpy(key+sec_ws_key.length(), WS_MAGIC_STRING, 36);

    SHA1(key, key_len, key_sha1);
    const std::string sec_ws_accept = util::base64_encode(key_sha1, SHA_DIGEST_LENGTH);

    http_response resp;
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

  void http_handler::_bad_request(std::shared_ptr<io::connection>& conn, const std::string reason) {
    http_response resp;
    resp.SetStatus(400, "Bad request");
    resp.SetHeader("connection", "close");
    resp.SetBody("<h1>400 Bad request</h1>\n" + reason + "\r\n");
    conn->write(resp.Get());
    conn->shutdown();
  }
}
