#include "common.hpp"
#include "connection.hpp"
#include "websocket_request.hpp"
#include "websocket_handler.hpp"

namespace eventhub {
  void websocket_handler::parse(std::shared_ptr<io::connection>& conn, char* buf, size_t n_bytes) {
    auto& req = conn->get_ws_request();
    req.parse(buf, n_bytes);

    switch(req.get_state()) {
      case websocket_request::WS_CONTROL_READY:
        _handle_control_frame(conn, req);
        req.clear_control_payload();
      break;

      case websocket_request::WS_DATA_READY:
        _handle_data_frame(conn, req);
        req.clear_payload();
      break;
    }
  }

  void websocket_handler::_handle_data_frame(std::shared_ptr<io::connection>& conn, websocket_request& req) {
    LOG(INFO) << "Data: " << req.get_payload();
  }

  void websocket_handler::_handle_control_frame(std::shared_ptr<io::connection>& conn, websocket_request& req) {
    LOG(INFO) << "Control Type: " << req.get_control_frame_type() << " payload: " << req.get_control_payload();
  }
}
