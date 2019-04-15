#ifndef EVENTHUB_SERVER_HPP
#define EVENTHUB_SERVER_HPP

#include <memory>
#include <mutex>
#include "worker.hpp"
#include "connection_worker.hpp"

using namespace std;

namespace eventhub {
    class server : public std::enable_shared_from_this<eventhub::server> {
      public:
        server();
        ~server();

        void start();
        void stop();
        const int get_server_socket();
        io::worker* get_worker();
        void publish(const std::string topic_name, const std::string data);

      private:
        int _server_socket;
        worker_group<io::worker> _connection_workers;
        worker_group<io::worker>::iterator _cur_worker;
        std::mutex _publish_lock;
    };
}

#endif
