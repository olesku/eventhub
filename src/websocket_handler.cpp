#include <memory>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <stdint.h>
#include "common.hpp"
#include "base64.hpp"
#include "connection.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "websocket_handler.hpp"

namespace eventhub {
  const std::string base64_encode(const unsigned char* buffer, size_t length) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    string s;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    s.assign(bufferPtr->data, bufferPtr->length);
    BIO_set_close(bio, BIO_CLOSE);
    BIO_free_all(bio);

    return s;
  }

  connection::state websocket_handler::handshake(const std::shared_ptr<connection>& conn) {
    auto& req = conn->get_http_request();

    const auto sec_ws_key = req.get_header("sec-websocket-key");
    if (req.get_header("upgrade").compare("websocket") != 0 || sec_ws_key.empty()) {
      _bad_request(conn);
      return conn->set_state(connection::state::WS_HANDSHAKE_FAILED);
    }

    size_t key_len = sec_ws_key.length() + 36;
    unsigned char key[key_len];
    unsigned char key_sha1[SHA_DIGEST_LENGTH] = {0};

    memcpy(key, sec_ws_key.c_str(), sec_ws_key.length());
    memcpy(key+sec_ws_key.length(), MAGIC_STRING, 36);

    SHA1(key, key_len, key_sha1);
    const std::string sec_ws_accept = base64_encode(key_sha1, SHA_DIGEST_LENGTH);

    http_response resp(101, "", false);
    resp.SetHeader("upgrade", "websocket");
    resp.SetHeader("connection", "upgrade");
    resp.SetHeader("sec-websocket-accept", sec_ws_accept);

    if (!req.get_header("Sec-WebSocket-Protocol").empty()) {
      resp.SetHeader("Sec-WebSocket-Protocol", req.get_header("Sec-WebSocket-Protocol"));
    }

    conn->write(resp.Get());
    return conn->set_state(connection::state::WS_PARSE);
  }

  void websocket_handler::_bad_request(const std::shared_ptr<connection>& conn) {
    http_response resp(400, "Bad request\r\n");
    conn->write(resp.Get());
    shutdown(conn->get_fd(), SHUT_RDWR);
  }

  connection::state websocket_handler::parse(const std::shared_ptr<connection>& conn, const char* buf, size_t buf_len) {
    
  }

}
