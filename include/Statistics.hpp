#ifndef INCLUDE_STATISTICS_HPP_
#define INCLUDE_STATISTICS_HPP_

namespace eventhub {
namespace statistics {

struct Worker {
  Worker() : current_connections_count(0),
             total_connect_count(0),
             total_disconnect_count(0),
             eventloop_delay(0) {};

  size_t current_connections_count;
  unsigned long long total_connect_count;
  unsigned long long total_disconnect_count;
  double eventloop_delay;
};

struct Server {
  Server() : worker_count(0), 
             publish_count(0),
             redis_connection_fail_count(0),
             redis_publish_delay(0) {};

  unsigned long server_start_unixtime;
  unsigned int worker_count;
  unsigned long long publish_count;
  unsigned int redis_connection_fail_count;
  double redis_publish_delay;
};

} // statisics
} // eventhub

#endif

