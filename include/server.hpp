#ifndef EVENTHUB_SERVER_HPP
#define EVENTHUB_SERVER_HPP

#include <memory>
#include "worker.hpp"
#include "connection_worker.hpp"

using namespace std;

namespace eventhub {
    class server : public std::enable_shared_from_this<eventhub::server> {
      public:
        server();
        ~server();

        std::shared_ptr<eventhub::server> shared_ptr() {
          return shared_from_this();
        }

        void start();
        void stop();
        const int get_server_socket();

      private:
        int _server_socket;
        worker_group<connection_worker> _connection_workers;
    };
}

#endif