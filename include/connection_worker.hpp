#ifndef EVENTHUB_CONNECTION_WORKER_HPP
#define EVENTHUB_CONNECTION_WORKER_HPP

#include <memory>
#include <vector>
#include <mutex>
#include "worker.hpp"
#include "connection.hpp"
#include "event_loop.hpp"
#include "topic_manager.hpp"

namespace eventhub {
  class server; // Forward declaration.

  namespace io {
    typedef std::unordered_map<unsigned int, std::shared_ptr<connection> > connection_list_t;

    class worker : public worker_base, public std::enable_shared_from_this<worker> {
      public:
        worker(std::shared_ptr<server> server);
        ~worker();

        std::shared_ptr<server>& get_server() { return _server; };
        topic_manager& get_topic_manager() { return _topic_manager; };

      private:
        std::shared_ptr<server> _server;
        int        _epoll_fd;
        event_loop _ev;
        connection_list_t _connection_list;
        std::mutex _connection_list_mutex;
        topic_manager _topic_manager;
        
        void _accept_connection();
        void _add_connection(int fd, struct sockaddr_in* csin);
        void _remove_connection(const connection_list_t::iterator& it);
        void _read(std::shared_ptr<connection>& conn);

        void worker_main();
    };
  }
}

#endif
