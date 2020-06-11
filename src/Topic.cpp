#include "Topic.hpp"

#include <memory>
#include <string>
#include <utility>
#include <stdlib.h>

#include "Common.hpp"
#include "EventLoop.hpp"
#include "Connection.hpp"
#include "websocket/Response.hpp"
#include "websocket/Types.hpp"
#include "sse/Response.hpp"
#include "ConnectionWorker.hpp"
#include "Config.hpp"

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

  unsigned int publish_delay_max = Config.getInt("MAX_RAND_PUBLISH_SPREAD_DELAY");

  try {
    jsonData = nlohmann::json::parse(data);

    for (auto subscriber : _subscriber_list) {
      auto c = subscriber.first.lock();

      if (!c || c->isShutdown()) {
        continue;
      }

      auto doPublish = [c, subscriber, jsonData]() {
        if (c->getState() == ConnectionState::WEBSOCKET) {
          websocket::response::sendData(c,
                                        jsonrpcpp::Response(subscriber.second, jsonData).to_json().dump(),
                                        websocket::FrameType::TEXT_FRAME);
        } else if (c->getState() == ConnectionState::SSE) {
          sse::response::sendEvent(c, jsonData["id"], jsonData["message"]);
        }
      };

      // If MAX_RAND_PUBLISH_SPREAD_DELAY is set then we
      // delay each publish with RANDOM % MAX_RAND_PUBLISH_SPREAD_DELAY (calculated per-client).
      // We implement this feature to prevent thundering herd issues.
      int64_t delay = publish_delay_max > 0 ? (rand() % publish_delay_max) : 0;

      if (delay > 0) {
        c->getWorker()->addTimer(delay, [doPublish](TimerCtx* ctx) {
          doPublish();
        });
      } else {
        // No delay is set, publish right away.
        doPublish();
      }
    }

  } catch (std::exception& e) {
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
