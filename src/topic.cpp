#include <memory>
#include "common.hpp"
#include "topic.hpp"
#include "connection.hpp"
#include "websocket_response.hpp"
#include <string>

using namespace std;

namespace eventhub {
  void topic::add_subscriber(std::shared_ptr<io::connection>& conn) {
    std::lock_guard<std::mutex> lock(_subscriber_lock);

    _subscriber_list.push_back(weak_ptr<io::connection>(conn));
    DLOG(INFO) << "Connection " << conn->get_ip() << " subscribed to " << _id;
  }

  void topic::publish(const string& data) {
    for (auto wptr_subscriber : _subscriber_list) {
      auto subscriber = wptr_subscriber.lock();

      if (!subscriber) {
        continue;
      }

      DLOG(INFO) << "Publish " << data;
      websocket::response ws_frame(data);
      subscriber->write(ws_frame.ws_format());
    }
  }

  size_t topic::garbage_collect() {
    std::lock_guard<std::mutex> lock(_subscriber_lock);

    for (auto it = _subscriber_list.begin(); it != _subscriber_list.end(); ) {
      if (!it->lock()) {
        it = _subscriber_list.erase(it);
      } else {
        it++;
      }
    }

    return _subscriber_list.size();
  }
}
