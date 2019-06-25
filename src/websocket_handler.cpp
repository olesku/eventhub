#include "websocket_handler.hpp"
#include "common.hpp"
#include "connection.hpp"
#include "websocket_request.hpp"

namespace eventhub {
void WebsocketHandler::parse(std::shared_ptr<Connection>& conn, char* buf, size_t n_bytes) {
  auto& req = conn->get_ws_request();
  req.parse(buf, n_bytes);

  switch (req.get_state()) {
    case WebsocketRequest::WS_CONTROL_READY:
      _handleControlFrame(conn, req);
      req.clearControlPayload();
      break;

    case WebsocketRequest::WS_DATA_READY:
      _handleDataFrame(conn, req);
      req.clearPayload();
      break;
  }
}

void WebsocketHandler::_handleDataFrame(std::shared_ptr<Connection>& conn, WebsocketRequest& req) {
  LOG(INFO) << "Data: " << req.get_payload();
}

void WebsocketHandler::_handleControlFrame(std::shared_ptr<Connection>& conn, WebsocketRequest& req) {
  LOG(INFO) << "Control Type: " << req.get_control_frame_type() << " payload: " << req.get_control_payload();
}
} // namespace eventhub
