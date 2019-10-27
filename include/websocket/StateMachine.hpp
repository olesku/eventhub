#ifndef EVENTHUB_WEBSOCKET_STATEMACHINE_HPP
#define EVENTHUB_WEBSOCKET_STATEMACHINE_HPP

#include <string>

#include "http_request.hpp"
#include "http_response.hpp"
#include "websocket/ws_parser.h"

namespace eventhub {
namespace websocket {

class StateMachine {
public:
  typedef enum {
    WS_PARSE,
    WS_CONTROL_READY,
    WS_DATA_READY
  } state;

  StateMachine();
  ~StateMachine() {};

  const state get_state();
  state set_state(state new_state);

  state process(char* buf, ssize_t len);

  void clearPayload();
  void clearControlPayload();
  void appendPayload(const char* data);
  void appendControlPayload(const char* data);
  void set_control_frame_type(uint8_t frame_type);

  const std::string& get_payload();
  const std::string& get_control_payload();
  uint8_t get_control_frame_type();

private:
  state _state;
  std::string _payload_buf;
  std::string _control_payload_buf;
  ws_parser_t _ws_parser;
  ws_parser_callbacks_t _ws_parser_callbacks;
  uint8_t _control_frame_type;
};

} // namespace websocket
} // namespace eventhub

#endif
