#ifndef INCLUDE_HANDLERCONTEXT_HPP_
#define INCLUDE_HANDLERCONTEXT_HPP_

#include <memory>

namespace eventhub {

class Server;
class Worker;
class Connection;

class HandlerContext {
  public:
  HandlerContext(Server* server, Worker* worker, std::shared_ptr<Connection> connection) :
    _server(server), _worker(worker), _connection(connection) {}

  ~HandlerContext() {}

  inline Server* server()                          { return _server; }
  inline Worker* worker()                          { return _worker; }
  inline std::shared_ptr<Connection>  connection() { return _connection; }

  private:
    class Server* _server;
    class  Worker* _worker;
    std::shared_ptr<class Connection> _connection;
};

} // namespace eventhub

#endif // INCLUDE_HANDLERCONTEXT_HPP_
