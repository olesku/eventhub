#ifndef EVENTHUB_WEBSOCKET_STATEMACHINE_HPP
#define EVENTHUB_WEBSOCKET_STATEMACHINE_HPP

#include <string>

#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
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
  ~StateMachine(){};

  const state getState();
  state setState(state newState);

  state process(char* buf, ssize_t len);

  void clearPayload();
  void clearControlPayload();
  void appendPayload(const char* data);
  void appendControlPayload(const char* data);
  void setControlFrameType(uint8_t frameType);

  const std::string& getPayload();
  const std::string& getControlPayload();
  uint8_t getControlFrameType();

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
