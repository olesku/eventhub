#ifndef EVENTHUB_WEBSOCKET_STATEMACHINE_HPP
#define EVENTHUB_WEBSOCKET_STATEMACHINE_HPP

#include <string>
#include "websocket/ws_parser.h"

namespace eventhub {
namespace websocket {

enum class State {
  PARSE,
  CONTROL_FRAME_READY,
  DATA_FRAME_READY
};

class StateMachine {
public:
  StateMachine();
  ~StateMachine(){};

  const State getState();
  void setState(State neState);

  State process(char* buf, ssize_t len);

  void clearPayload();
  void clearControlPayload();
  void appendPayload(const char* data);
  void appendControlPayload(const char* data);
  void setControlFrameType(uint8_t frameType);

  const std::string& getPayload();
  const std::string& getControlPayload();
  uint8_t getControlFrameType();

private:
  State _state;
  std::string _payload_buf;
  std::string _control_payload_buf;
  ws_parser_t _ws_parser;
  ws_parser_callbacks_t _ws_parser_callbacks;
  uint8_t _control_frame_type;
};

} // namespace websocket
} // namespace eventhub

#endif
