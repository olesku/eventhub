#include "websocket/Parser.hpp"

#include <string>

#include "websocket/Types.hpp"
#include "Common.hpp"
#include "websocket/ws_parser.h"

namespace eventhub {
namespace websocket {
static int parserOnDataBegin(void* userData, uint8_t frameType) {
  //DLOG(INFO) << "on_data_begin";
  auto obj = static_cast<Parser*>(userData);
  obj->clearDataPayload();
  obj->setDataFrameType((FrameType)frameType);
  return 0;
}

static int parserOnDataPayload(void* userData, const char* buff, size_t len) {
  //DLOG(INFO) << "on_data_payload: " << buff << " len: " << len;
  auto obj = static_cast<Parser*>(userData);
  obj->appendDataPayload(buff, len);
  return 0;
}

static int parserOnDataEnd(void* userData) {
  //DLOG(INFO) << "on_data_end";
  auto obj = static_cast<Parser*>(userData);
  obj->callback(ParserStatus::PARSER_OK, obj->getDataFrameType(), obj->getDataPayload());
  return 0;
}

static int parserOnControlBegin(void* userData, uint8_t frameType) {
  auto obj = static_cast<Parser*>(userData);
  //DLOG(INFO) << "on_control_begin";
  obj->clearControlPayload();
  obj->setControlFrameType((FrameType)frameType);
  return 0;
}

static int parserOnControlPayload(void* userData, const char* buff, size_t len) {
  //DLOG(INFO) << "on_control_payload";
  auto obj = static_cast<Parser*>(userData);
  obj->appendControlPayload(buff, len);
  return 0;
}

static int parserOnControlEnd(void* userData) {
  auto obj = static_cast<Parser*>(userData);
  obj->callback(ParserStatus::PARSER_OK, obj->getControlFrameType(), obj->getControlPayload());
  //LOG(INFO) << "on_control_end";
  return 0;
}

Parser::Parser() {
  _callback = [](ParserStatus status, FrameType frameType, const std::string& data){};

  ws_parser_init(&_ws_parser);

  _ws_parser_callbacks = {
      .on_data_begin      = parserOnDataBegin,
      .on_data_payload    = parserOnDataPayload,
      .on_data_end        = parserOnDataEnd,
      .on_control_begin   = parserOnControlBegin,
      .on_control_payload = parserOnControlPayload,
      .on_control_end     = parserOnControlEnd,
  };
}

void Parser::clearDataPayload() {
  _data_payload_buf.clear();
}

void Parser::clearControlPayload() {
  _control_payload_buf.clear();
}

void Parser::appendDataPayload(const char* data, size_t len) {
  _data_payload_buf.insert(_data_payload_buf.size(), data, len);

  if (_data_payload_buf.size() > WS_MAX_DATA_FRAME_SIZE) {
    _callback(ParserStatus::MAX_DATA_FRAME_SIZE_EXCEEDED, _data_frame_type, _data_payload_buf);
  }
}

void Parser::appendControlPayload(const char* data, size_t len) {
  _control_payload_buf.insert(_control_payload_buf.size(), data, len);

  if (_control_payload_buf.size() > WS_MAX_CONTROL_FRAME_SIZE) {
    _callback(ParserStatus::MAX_CONTROL_FRAME_SIZE_EXCEEDED, _control_frame_type, _data_payload_buf);
  }
}

void Parser::setControlFrameType(FrameType frameType) {
  _control_frame_type = frameType;
}

void Parser::setDataFrameType(FrameType frameType) {
  _data_frame_type = frameType;
}

FrameType Parser::getControlFrameType() {
  return _control_frame_type;
}

FrameType Parser::getDataFrameType() {
  return _data_frame_type;
}

const std::string& Parser::getDataPayload() {
  return _data_payload_buf;
}

const std::string& Parser::getControlPayload() {
  return _control_payload_buf;
}

void Parser::parse(char* buf, size_t len) {
  ws_parser_execute(&_ws_parser, &_ws_parser_callbacks, this, buf, len);
}

} // namespace websocket
} // namespace eventhub
