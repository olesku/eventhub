#ifndef EVENTHUB_WEBSOCKET_REQUEST_HPP
#define EVENTHUB_WEBSOCKET_REQUEST_HPP

#include <string>

#include "http_request.hpp"
#include "http_response.hpp"
#include "ws_parser.h"


namespace eventhub {
  class websocket_request {
    public:
      typedef enum {
        WS_PARSE,
        WS_CONTROL_READY,
        WS_DATA_READY
      } state;
  
      websocket_request();
      ~websocket_request() {}

      const state get_state();
      state set_state(state new_state);

      state parse(char* buf, ssize_t len);

      void clear_payload();
      void clear_control_payload();
      void append_payload(const char* data);
      void append_control_payload(const char* data);
      void set_control_frame_type(uint8_t frame_type);

      const std::string& get_payload();
      const std::string& get_control_payload();
      uint8_t            get_control_frame_type();

    private:
      state _state;
      std::string _payload_buf;
      std::string _control_payload_buf;
      ws_parser_t _ws_parser;
      ws_parser_callbacks_t _ws_parser_callbacks;
      uint8_t _control_frame_type;
  };
}

#endif
