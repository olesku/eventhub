#ifndef INCLUDE_HANDLERCONTEXT_HPP_
#define INCLUDE_HANDLERCONTEXT_HPP_

#include <memory>
#include "Forward.hpp"

namespace eventhub {

class HandlerContext {
public:
  HandlerContext(Server* server, Worker* worker, std::shared_ptr<Connection> connection) : _server(server), _worker(worker), _connection(connection) {}

  ~HandlerContext() {}

  inline Server* server() { return _server; }
  inline Worker* worker() { return _worker; }
  inline std::shared_ptr<Connection> connection() { return _connection; }

private:
  Server* _server;
  Worker* _worker;
  std::shared_ptr<Connection> _connection;
};

} // namespace eventhub

#endif // INCLUDE_HANDLERCONTEXT_HPP_
