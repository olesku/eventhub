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
#include "websocket_request.hpp"
#include "ws_parser.h"

namespace eventhub {
    namespace websocket {
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

    static int parser_on_data_begin(void* user_data, uint8_t frame_type) {
      DLOG(INFO) << "on_data_begin";
      auto obj = static_cast<request*>(user_data);

      obj->clear_payload();
      return 0;
    }

    static int parser_on_data_payload(void* user_data, const char* buff, size_t len) {
      DLOG(INFO) << "on_data_payload: " << buff;
      auto obj = static_cast<request*>(user_data);
      obj->append_payload(buff);
      return 0;
    }

    static int parser_on_data_end(void* user_data) {
      DLOG(INFO) << "on_data_end";
      auto obj = static_cast<request*>(user_data);
      obj->set_state(request::WS_DATA_READY);
      return 0;
    }

    static int parser_on_control_begin(void* user_data, uint8_t frame_type) {
      auto obj = static_cast<request*>(user_data);
      DLOG(INFO) << "on_control_begin";
      obj->clear_control_payload();
      obj->set_control_frame_type(frame_type);
      return 0;
    }

    static int parser_on_control_payload(void* user_data, const char* buff, size_t len) {
      DLOG(INFO) << "on_control_payload";
      auto obj = static_cast<request*>(user_data);
      obj->append_control_payload(buff);
      return 0;
    }

    static int parser_on_control_end(void* user_data) {
      auto obj = static_cast<request*>(user_data);
      obj->set_state(request::state::WS_CONTROL_READY);
      DLOG(INFO) << "on_control_end";
      return 0;
    }

    request::request() {
      ws_parser_init(&_ws_parser);

       _ws_parser_callbacks = {
        .on_data_begin      = parser_on_data_begin,
        .on_data_payload    = parser_on_data_payload,
        .on_data_end        = parser_on_data_end,
        .on_control_begin   = parser_on_control_begin,
        .on_control_payload = parser_on_control_payload,
        .on_control_end     = parser_on_control_end,
      };

      set_state(state::WS_HANDSHAKE);
    }

    const request::state request::get_state() { 
      return _state;   
    }

    request::state request::set_state(state new_state) { 
      return _state = new_state; 
    }

    void request::clear_payload()  { 
      _payload_buf.clear(); 
    }

    void request::clear_control_payload()  { 
      _control_payload_buf.clear();
    }

    void request::append_payload(const char* data) { 
      _payload_buf.append(data);
    }
    
    void request::append_control_payload(const char* data) { 
      _control_payload_buf.append(data);  
    }

    void request::set_control_frame_type(uint8_t frame_type) {
      _control_frame_type = frame_type;
    }

    const std::string& request::get_payload() {
      return _payload_buf;
    }
    
    const std::string& request::get_control_payload() {
      return _control_payload_buf;
    }

    uint8_t request::get_control_frame_type() {
      return _control_frame_type;
    }

    std::shared_ptr<http_response> request::make_handshake_response(http_request& req) {
      const auto sec_ws_key = req.get_header("sec-websocket-key");
      auto resp = std::make_shared<http_response>();

      if (req.get_header("upgrade").compare("websocket") != 0 || sec_ws_key.empty()) {
        resp->SetStatus(400, "Bad request");
        resp->SetHeader("connection", "close");
        resp->SetBody("Bad request\r\n");
        set_state(WS_BAD_HANDSHAKE);

        return resp;
      }

      size_t key_len = sec_ws_key.length() + 36;
      unsigned char key[key_len];
      unsigned char key_sha1[SHA_DIGEST_LENGTH] = {0};

      memcpy(key, sec_ws_key.c_str(), sec_ws_key.length());
      memcpy(key+sec_ws_key.length(), MAGIC_STRING, 36);

      SHA1(key, key_len, key_sha1);
      const std::string sec_ws_accept = base64_encode(key_sha1, SHA_DIGEST_LENGTH);

      resp->SetStatus(101);
      resp->SetHeader("upgrade", "websocket");
      resp->SetHeader("connection", "upgrade");
      resp->SetHeader("sec-websocket-accept", sec_ws_accept);

      if (!req.get_header("Sec-WebSocket-Protocol").empty()) {
        resp->SetHeader("Sec-WebSocket-Protocol", req.get_header("Sec-WebSocket-Protocol"));
      }

      set_state(WS_HANDSHAKE_OK);
      return resp;
    }

    request::state request::parse(char* buf, ssize_t len) {
      ws_parser_execute(&_ws_parser, &_ws_parser_callbacks, this, buf, len);
      return _state;
    }
  }
} 
