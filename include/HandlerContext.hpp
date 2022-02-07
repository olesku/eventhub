#pragma once

#include <memory>

#include "Forward.hpp"
#include "Server.hpp"
#include "EventhubBase.hpp"

namespace eventhub {

class HandlerContext final : public EventhubBase {
public:
  HandlerContext(Server* server, Worker* worker, std::shared_ptr<Connection> connection) :
    EventhubBase(server->config()), _server(server), _worker(worker), _connection(connection) {};

  ~HandlerContext() {}

  Server* server() { return _server; }
  Worker* worker() { return _worker; }
  std::shared_ptr<Connection> connection() { return _connection; }

private:
  Server* _server;
  Worker* _worker;
  std::shared_ptr<Connection> _connection;
};

} // namespace eventhub


