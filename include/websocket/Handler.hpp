#ifndef EVENTHUB_WEBSOCKET_HANDLER_HPP
#define EVENTHUB_WEBSOCKET_HANDLER_HPP

#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "websocket/StateMachine.hpp"
#include <memory>

namespace eventhub {
namespace websocket {

class Handler {
public:
  static void process(ConnectionPtr conn, char* buf, size_t nBytes);

private:
  Handler(){};
  ~Handler(){};

  static void _handleDataFrame(ConnectionPtr conn);
  static void _handleControlFrame(ConnectionPtr conn);
  static void _handleClientCommand(ConnectionPtr conn, const std::string& command, const std::string& args);
};

} // namespace websocket
} // namespace eventhub

#endif
