#ifndef EVENTHUB_WEBSOCKET_HANDLER_HPP
#define EVENTHUB_WEBSOCKET_HANDLER_HPP

#include "http_request.hpp"
#include "http_response.hpp"
#include "ws_parser.h"
#include <memory>

namespace eventhub {
  namespace websocket {
    #define MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

    class request {
      public:
        typedef enum {
          WS_HANDSHAKE,
          WS_HANDSHAKE_OK,
          WS_BAD_HANDSHAKE,
          WS_PARSE,
          WS_CONTROL_READY,
          WS_DATA_READY
        } state;
    
        request();
        ~request() {}

        const state get_state();
        state set_state(state new_state);

        std::shared_ptr<http_response>  make_handshake_response(http_request& req);
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
}

#endif
