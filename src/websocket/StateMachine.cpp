#include "websocket/StateMachine.hpp"

#include <string>
#include "common.hpp"
#include "websocket/ws_parser.h"

namespace eventhub {
namespace websocket {
static int parser_on_data_begin(void* user_data, uint8_t frame_type) {
  //DLOG(INFO) << "on_data_begin";
  auto obj = static_cast<StateMachine*>(user_data);

  obj->clearPayload();
  return 0;
}

static int parser_on_data_payload(void* user_data, const char* buff, size_t len) {
  //DLOG(INFO) << "on_data_payload: " << buff;
  auto obj = static_cast<StateMachine*>(user_data);
  obj->appendPayload(buff);
  return 0;
}

static int parser_on_data_end(void* user_data) {
  //DLOG(INFO) << "on_data_end";
  auto obj = static_cast<StateMachine*>(user_data);
  obj->set_state(StateMachine::WS_DATA_READY);
  return 0;
}

static int parser_on_control_begin(void* user_data, uint8_t frame_type) {
  auto obj = static_cast<StateMachine*>(user_data);
  //DLOG(INFO) << "on_control_begin";
  obj->clearControlPayload();
  obj->set_control_frame_type(frame_type);
  return 0;
}

static int parser_on_control_payload(void* user_data, const char* buff, size_t len) {
  //DLOG(INFO) << "on_control_payload";
  auto obj = static_cast<StateMachine*>(user_data);
  obj->appendControlPayload(buff);
  return 0;
}

static int parser_on_control_end(void* user_data) {
  auto obj = static_cast<StateMachine*>(user_data);
  obj->set_state(StateMachine::state::WS_CONTROL_READY);
  //DLOG(INFO) << "on_control_end";
  return 0;
}

StateMachine::StateMachine() {
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

const StateMachine::state StateMachine::get_state() {
  return _state;
}

StateMachine::state StateMachine::set_state(state new_state) {
  return _state = new_state;
}

void StateMachine::clearPayload() {
  _payload_buf.clear();
  set_state(state::WS_PARSE);
}

void StateMachine::clearControlPayload() {
  _control_payload_buf.clear();
  set_state(state::WS_PARSE);
}

void StateMachine::appendPayload(const char* data) {
  _payload_buf.append(data);
}

void StateMachine::appendControlPayload(const char* data) {
  _control_payload_buf.append(data);
}

void StateMachine::set_control_frame_type(uint8_t frame_type) {
  _control_frame_type = frame_type;
}

const std::string& StateMachine::get_payload() {
  return _payload_buf;
}

const std::string& StateMachine::get_control_payload() {
  return _control_payload_buf;
}

uint8_t StateMachine::get_control_frame_type() {
  return _control_frame_type;
}

StateMachine::state StateMachine::process(char* buf, ssize_t len) {
  ws_parser_execute(&_ws_parser, &_ws_parser_callbacks, this, buf, len);
  return _state;
}

}
} // namespace eventhub
