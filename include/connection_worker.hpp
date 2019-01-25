#ifndef EVENTHUB_CONNECTION_WORKER_HPP
#define EVENTHUB_CONNECTION_WORKER_HPP

#include <memory>
#include <vector>
#include <mutex>
#include "connection.hpp"

namespace eventhub {
  class server; // Forward declaration.

  class connection_worker : public worker {
    public:
      connection_worker(std::shared_ptr<server> server);
      ~connection_worker();

    private:
      std::shared_ptr<server> _server;
      int        _epoll_fd;

      eventhub::connection_list _connection_list;
      std::mutex _connection_list_mutex;

      void _new_connection(int fd, struct sockaddr_in* csin);
      void _accept_connection();
      void _read(std::shared_ptr<eventhub::connection> conn, eventhub::connection_list::iterator it);

      void worker_main();
  };
}

#endif
