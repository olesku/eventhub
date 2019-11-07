#include "Topic.hpp"

#include <memory>
#include <string>
#include <utility>

#include "Common.hpp"
#include "Connection.hpp"
#include "websocket/Response.hpp"
#include "websocket/Types.hpp"

using namespace std;

namespace eventhub {
Topic::~Topic() {}

/**
 * Add a subscriber to this Topic.
 * @param conn Connection to add.
 * @param subscriptionRequestId ID from JSONRPC call to publish().
 */
TopicSubscriberList::iterator Topic::addSubscriber(ConnectionPtr conn, const jsonrpcpp::Id subscriptionRequestId) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  return _subscriber_list.insert(_subscriber_list.begin(), std::make_pair(ConnectionWeakPtr(conn), subscriptionRequestId));
}

/**
 * Publish a message to this topic.
 * @param data Message to publish.
 */
void Topic::publish(const string& data) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  nlohmann::json jsonData;

  try {
    jsonData = nlohmann::json::parse(data);

    for (auto subscriber : _subscriber_list) {
      auto c = subscriber.first.lock();

      if (!c || c->isShutdown()) {
        continue;
      }

      websocket::response::sendData(c,
                                    jsonrpcpp::Response(subscriber.second, jsonData).to_json().dump(),
                                    websocket::FrameType::TEXT_FRAME);
    }
  }

  catch (std::exception& e) {
    DLOG(INFO) << "Invalid publish to " << _id << ": " << e.what();
    return;
  }
}

/**
 * Delete a subscriber.
 * @param it Iterator pointing to the subscriber to be deleted.
 *           This is obtained by call to addSubscriber.
 */
void Topic::deleteSubscriberByIterator(TopicSubscriberList::iterator it) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  _subscriber_list.erase(it);
}

} // namespace eventhub
