#include "Topic.hpp"
#include "Common.hpp"
#include "Connection.hpp"
#include "websocket/Response.hpp"
#include <memory>
#include <string>

using namespace std;

namespace eventhub {
Topic::~Topic() {
  LOG(INFO) << "Topic: " << _id << " destructor.";
}

TopicSubscriberList::iterator Topic::addSubscriber(ConnectionPtr conn) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  return _subscriber_list.emplace(_subscriber_list.begin(), ConnectionWeakPtr(conn));
}

void Topic::publish(const string& data) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);

  for (auto subscriber : _subscriber_list) {
    auto c = subscriber.lock();

    if (!c || c->isShutdown()) {
      continue;
    }

    websocket::response::sendData(c, data, websocket::response::TEXT_FRAME);
  }
}

void Topic::deleteSubscriberByIterator(TopicSubscriberList::iterator it) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  _subscriber_list.erase(it);
}

} // namespace eventhub
