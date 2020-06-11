#include "Topic.hpp"

#include <memory>
#include <string>
#include <utility>

#include "Common.hpp"
#include "Connection.hpp"
#include "websocket/Response.hpp"
#include "websocket/Types.hpp"
#include "sse/Response.hpp"

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
 * Helper function for publish()
*/
void Topic::_doPublish(ConnectionWeakPtr c, const jsonrpcpp::Id jsonRpcId, const nlohmann::json jsonData) {
  auto conn = c.lock();

  if (!conn || conn->isShutdown()) {
        return;
  }

  if (conn->getState() == ConnectionState::WEBSOCKET) {
    websocket::response::sendData(conn,
                                    jsonrpcpp::Response(jsonRpcId, jsonData).to_json().dump(),
                                    websocket::FrameType::TEXT_FRAME);
  } else if (conn->getState() == ConnectionState::SSE) {
      sse::response::sendEvent(conn, jsonData["id"], jsonData["message"]);
  }
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

<<<<<<< HEAD
      if (c->getState() == ConnectionState::WEBSOCKET) {
        websocket::response::sendData(c,
                                      jsonrpcpp::Response(subscriber.second, jsonData).to_json().dump(),
                                      websocket::FrameType::TEXT_FRAME);
      } else if (c->getState() == ConnectionState::SSE) {
        sse::response::sendEvent(c, jsonData["id"], jsonData["message"]);
=======
      // If MAX_RAND_PUBLISH_SPREAD_DELAY is set then we
      // delay each publish with RANDOM % MAX_RAND_PUBLISH_SPREAD_DELAY (calculated per-client).
      // We implement this feature to prevent thundering herd issues.
      int64_t delay = publish_delay_max > 0 ? (rand() % publish_delay_max) : 0;

      if (delay > 0) {
        c->getWorker()->addTimer(delay, [subscriber, jsonData](TimerCtx* ctx) {
          _doPublish(subscriber.first, subscriber.second, jsonData);
        });
      } else {
        // No delay is set, publish right away.
        _doPublish(subscriber.first, subscriber.second, jsonData);
>>>>>>> 70cef26... Fix race condition.
      }
    }
  }

  catch (std::exception& e) {
    LOG->debug("Invalid publish to {}: {}.", _id, e.what());
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
