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
#include "http_response.hpp"
#include "websocket_handler.hpp"

namespace eventhub {
const std::string Base64Encode(const unsigned char* buffer, size_t length) {
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

  websocket_handler::websocket_handler() {

  }

  websocket_handler::~websocket_handler() {

  }

  bool websocket_handler::handshake(const std::shared_ptr<connection>& conn) {
    const auto sec_ws_key = conn->get_http_request().get_header("sec-websocket-key");
    if (sec_ws_key.empty()) {
      _bad_request(conn);
      return false;
    }

    auto key = sec_ws_key + MAGIC_STRING;
    DLOG(INFO) << "key: " << key;


    unsigned char* sha_str = SHA1(reinterpret_cast<const unsigned char*>(&key[0]), key.length(), nullptr);
    if (strlen(reinterpret_cast<const char*>(sha_str)) != 20) {
      DLOG(INFO) << "Invalid SHA1";
      _bad_request(conn);
      return false;
    }

    const std::string sec_ws_accept = Base64Encode(sha_str, 20);

    DLOG(INFO) << "sec_ws_accept: " << sec_ws_accept;

    http_response resp(101, "", false);
    resp.SetHeader("upgrade", "websocket");
    resp.SetHeader("connection", "upgrade");
    if (!conn->get_http_request().get_header("Sec-WebSocket-Protocol").empty()) {
      resp.SetHeader("Sec-WebSocket-Protocol", conn->get_http_request().get_header("Sec-WebSocket-Protocol"));
    }
    resp.SetHeader("sec-websocket-accept", sec_ws_accept);

    conn->write(resp.Get());

    conn->set_state(connection::state::WS_PARSE_OK);

    return true;
  }

  void websocket_handler::_bad_request(const std::shared_ptr<connection>& conn) {
    http_response resp(400, "Bad request\r\n");
    conn->write(resp.Get());
    shutdown(conn->get_fd(), SHUT_RDWR);
  }
}
