#ifndef EVENTHUB_WEBSOCKET_PARSER_HPP
#define EVENTHUB_WEBSOCKET_PARSER_HPP

#include "websocket/ws_parser.h"
#include "websocket/Types.hpp"
#include <string>
#include <functional>

namespace eventhub {
namespace websocket {

class Parser {
public:
  Parser();
  ~Parser(){};

  void parse(char* buf, size_t len);

  void clearDataPayload();
  void clearControlPayload();

  void appendDataPayload(const char* data, size_t len);
  void appendControlPayload(const char* data, size_t len);

  void setControlFrameType(FrameType frameType);
  void setDataFrameType(FrameType frameType);

  const std::string& getDataPayload();
  const std::string& getControlPayload();

  FrameType getControlFrameType();
  FrameType getDataFrameType();

  inline void callback(ParserStatus status, FrameType frameType, const std::string& data) { _callback(status, frameType, data); }
  inline void setCallback(ParserCallback callback) { _callback = callback; }

private:
  std::string _data_payload_buf;
  std::string _control_payload_buf;
  ws_parser_t _ws_parser;
  ws_parser_callbacks_t _ws_parser_callbacks;
  FrameType _data_frame_type;
  FrameType _control_frame_type;
  ParserCallback _callback;
};

} // namespace websocket
} // namespace eventhub

#endif
