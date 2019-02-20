#include <memory>
#include "topic.hpp"
#include "connection.hpp"
#include <string>

using namespace std;

namespace eventhub {
  void topic::add_subscriber(std::shared_ptr<io::connection>& conn) {
    _subscriber_list.push_back(weak_ptr<io::connection>(conn));
  }

  void topic::publish(const string& data) {
    for (auto wptr_subscriber : _subscriber_list) {
      auto subscriber = wptr_subscriber.lock();

      if (!subscriber) {
        continue;
      }

      subscriber->write(data);
    }
  }

  size_t topic::garbage_collect() {
    for (auto it = _subscriber_list.begin(); it != _subscriber_list.end(); ) {
      if (!it->lock()) {

      }
    }

    return _subscriber_list.size();
  }
}
