#ifndef EVENTHUB_CONNECTION_WORKER_HPP
#define EVENTHUB_CONNECTION_WORKER_HPP

#include <memory>
#include <vector>
#include <mutex>
#include "worker.hpp"
#include "connection.hpp"
#include "event_loop.hpp"

namespace eventhub {
  class server; // Forward declaration.

  namespace io {
    typedef std::unordered_map<unsigned int, std::shared_ptr<connection> > connection_list;

    class worker : public worker_base {
      public:
        worker(std::shared_ptr<server> server);
        ~worker();

        std::shared_ptr<server>& get_server() { return _server; };

      private:
        std::shared_ptr<server> _server;
        int        _epoll_fd;
        event_loop _ev;

        connection_list _connection_list;
        std::mutex _connection_list_mutex;

        void _accept_connection();
        void _add_connection(int fd, struct sockaddr_in* csin);
        void _remove_connection(const connection_list::iterator& it);
        void _read(std::shared_ptr<connection>& conn);

        void worker_main();
    };
  }
}

#endif
