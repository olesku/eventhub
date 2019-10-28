#include "Topic.hpp"
#include "Common.hpp"
#include "Connection.hpp"
#include "websocket/Response.hpp"
#include <memory>
#include <string>

using namespace std;

namespace eventhub {
void Topic::addSubscriber(ConnectionPtr conn) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);

  _subscriber_list.push_back(weak_ptr<Connection>(conn));
  DLOG(INFO) << "Connection " << conn->getIP() << " subscribed to " << _id;
}

void Topic::publish(const string& data) {
  for (auto wptrSubscriber : _subscriber_list) {
    auto subscriber = wptrSubscriber.lock();

    if (!subscriber) {
      continue;
    }

    DLOG(INFO) << "Publish " << data;
    websocket::response::sendData(subscriber, data);
  }
}

size_t Topic::garbageCollect() {
  std::lock_guard<std::mutex> lock(_subscriber_lock);

  for (auto it = _subscriber_list.begin(); it != _subscriber_list.end();) {
    if (!it->lock()) {
      it = _subscriber_list.erase(it);
    } else {
      it++;
    }
  }

  return _subscriber_list.size();
}
} // namespace eventhub
