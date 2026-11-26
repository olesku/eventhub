#include <spdlog/logger.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <exception>
#include <initializer_list>
#include <vector>

#include "Topic.hpp"
#include "Connection.hpp"
#include "PublishedEvent.hpp"
#include "sse/Response.hpp"
#include "websocket/Response.hpp"
#include "Logger.hpp"
#include "jwt/json/json.hpp"

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
 * @param event Shared message to publish.
 */
void Topic::publish(const PublishedEvent& event) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);

  try {
    if (_subscriber_list.empty()) {
      return;
    }
    const auto& jsonData = event.json();
    if (jsonData.is_discarded()) {
      LOG->debug("Invalid publish to {}: expected JSON.", _id);
      return;
    }

    for (const auto& subscriber : _subscriber_list) {
      auto c = subscriber.first.lock();

      if (!c || c->isShutdown()) {
        continue;
      }

      // One subscriber failing must not keep the event from the rest.
      try {
        if (c->getState() == ConnectionState::WEBSOCKET) {
          websocket::Response::sendEvent(*c, subscriber.second, event);
        } else if (c->getState() == ConnectionState::SSE) {
          sse::Response::sendEvent(c, jsonData.at("id"), jsonData.at("message"));
        }
      } catch (std::exception& e) {
        LOG->debug("Failed to deliver publish to {} for {}: {}.", _id, c->getIP(), e.what());
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

/**
 * Returns the number of subscribers on the topic.
 */
std::size_t Topic::getSubscriberCount() {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  return _subscriber_list.size();
}

} // namespace eventhub
