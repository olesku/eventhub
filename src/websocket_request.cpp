#include <string>

#include "common.hpp"
#include "websocket_request.hpp"
#include "ws_parser.h"

namespace eventhub {
static int parser_on_data_begin(void* user_data, uint8_t frame_type) {
  //DLOG(INFO) << "on_data_begin";
  auto obj = static_cast<WebsocketRequest*>(user_data);

  obj->clearPayload();
  return 0;
}

static int parser_on_data_payload(void* user_data, const char* buff, size_t len) {
  //DLOG(INFO) << "on_data_payload: " << buff;
  auto obj = static_cast<WebsocketRequest*>(user_data);
  obj->appendPayload(buff);
  return 0;
}

static int parser_on_data_end(void* user_data) {
  //DLOG(INFO) << "on_data_end";
  auto obj = static_cast<WebsocketRequest*>(user_data);
  obj->set_state(WebsocketRequest::WS_DATA_READY);
  return 0;
}

static int parser_on_control_begin(void* user_data, uint8_t frame_type) {
  auto obj = static_cast<WebsocketRequest*>(user_data);
  //DLOG(INFO) << "on_control_begin";
  obj->clearControlPayload();
  obj->set_control_frame_type(frame_type);
  return 0;
}

static int parser_on_control_payload(void* user_data, const char* buff, size_t len) {
  //DLOG(INFO) << "on_control_payload";
  auto obj = static_cast<WebsocketRequest*>(user_data);
  obj->appendControlPayload(buff);
  return 0;
}

static int parser_on_control_end(void* user_data) {
  auto obj = static_cast<WebsocketRequest*>(user_data);
  obj->set_state(WebsocketRequest::state::WS_CONTROL_READY);
  //DLOG(INFO) << "on_control_end";
  return 0;
}

WebsocketRequest::WebsocketRequest() {
  ws_parser_init(&_ws_parser);

  _ws_parser_callbacks = {
      .on_data_begin      = parser_on_data_begin,
      .on_data_payload    = parser_on_data_payload,
      .on_data_end        = parser_on_data_end,
      .on_control_begin   = parser_on_control_begin,
      .on_control_payload = parser_on_control_payload,
      .on_control_end     = parser_on_control_end,
  };

  set_state(state::WS_PARSE);
}

const WebsocketRequest::state WebsocketRequest::get_state() {
  return _state;
}

WebsocketRequest::state WebsocketRequest::set_state(state new_state) {
  return _state = new_state;
}

void WebsocketRequest::clearPayload() {
  _payload_buf.clear();
  set_state(state::WS_PARSE);
}

void WebsocketRequest::clearControlPayload() {
  _control_payload_buf.clear();
  set_state(state::WS_PARSE);
}

void WebsocketRequest::appendPayload(const char* data) {
  _payload_buf.append(data);
}

void WebsocketRequest::appendControlPayload(const char* data) {
  _control_payload_buf.append(data);
}

void WebsocketRequest::set_control_frame_type(uint8_t frame_type) {
  _control_frame_type = frame_type;
}

const std::string& WebsocketRequest::get_payload() {
  return _payload_buf;
}

const std::string& WebsocketRequest::get_control_payload() {
  return _control_payload_buf;
}

uint8_t WebsocketRequest::get_control_frame_type() {
  return _control_frame_type;
}

WebsocketRequest::state WebsocketRequest::parse(char* buf, ssize_t len) {
  ws_parser_execute(&_ws_parser, &_ws_parser_callbacks, this, buf, len);
  return _state;
}
} // namespace eventhub
