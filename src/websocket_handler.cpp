#include "websocket_handler.hpp"
#include "common.hpp"
#include "connection.hpp"
#include "websocket_request.hpp"
#include "topic_manager.hpp"

namespace eventhub {
void WebsocketHandler::parse(std::shared_ptr<Connection>& conn, Worker* wrk, char* buf, size_t n_bytes) {
  auto& req = conn->get_ws_request();
  req.parse(buf, n_bytes);

  switch (req.get_state()) {
    case WebsocketRequest::WS_CONTROL_READY:
      _handleControlFrame(conn, wrk, req);
      req.clearControlPayload();
      break;

    case WebsocketRequest::WS_DATA_READY:
      _handleDataFrame(conn, wrk, req);
      req.clearPayload();
      break;
  }
}

void WebsocketHandler::_handleDataFrame(std::shared_ptr<Connection>& conn, Worker* wrk, WebsocketRequest& req) {
  auto& payload = req.get_payload();

  LOG(INFO) << "Data: " << payload;
  
  // Parse command and arguments.
  // Format is <COMMAND><SPACE><ARGUMENTS>CRLF
  if (payload.length() > 2 && payload.substr(payload.length()-2, payload.length()).compare("\r\n") == 0) {
    std::string command;
    std::string arg;
    auto argPos = payload.find_first_of(' ');
    
    if (argPos != std::string::npos) {
      command = payload.substr(0, argPos);   
      arg = payload.substr(argPos+1, payload.length()-command.length()-3);
    } else {
      command = payload.substr(0, payload.length()-2);
    }

    _handleClientCommand(conn, wrk, command, arg);
  }
}

void WebsocketHandler::_handleControlFrame(std::shared_ptr<Connection>& conn, Worker* wrk, WebsocketRequest& req) {
  // TODO: Handle pings.
  LOG(INFO) << "Control Type: " << req.get_control_frame_type() << " payload: " << req.get_control_payload();
}

void WebsocketHandler::_handleClientCommand(std::shared_ptr<Connection>& conn, Worker* wrk, const std::string& command, const std::string& arg) {
  LOG(INFO) << "Command: '" << command << "'";
  LOG(INFO) << "Arg: '" << arg << "'";

  // Subscribe to a topic.
  if (command.compare("SUBSCRIBE") == 0) {
    if (TopicManager::isValidTopicFilter(arg)) {
      wrk->subscribeConnection(conn, arg);
      // TODO: Check if client has access to subscribe to the topic.
      LOG(INFO) << "Valid topic " << arg;
    } else {
      // Not a valid topic.
      LOG(INFO) << "Invalid topic " << arg;
    }
  } 
  
  // Unsubscribe from a channel.
  else if (command.compare("UNSUBSCRIBE") == 0) {

  }

  // Unsubscribe from all channels.
  else if (command.compare("UNSUBSCRIBEALL") == 0) {

  }

  // Publish to a channel.
  else if (command.compare("PUBLISH") == 0) {

  }

  // List all subscribed channels.
  else if (command.compare("LIST") == 0) {

  }
}
} // namespace eventhub
