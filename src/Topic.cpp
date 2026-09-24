#include <exception>
#include <initializer_list>
#include <memory>
#include <spdlog/logger.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "Connection.hpp"
#include "Logger.hpp"
#include "Topic.hpp"
#include "jwt/json/json.hpp"
#include "sse/Response.hpp"
#include "websocket/Response.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
Topic::~Topic() {}

/**
 * Add a subscriber to this Topic.
 * @param conn Connection to add.
 * @param subscriptionRequestId ID from JSONRPC call to publish().
 */
SubscriptionId Topic::addSubscriber(ConnectionPtr connection, const jsonrpcpp::Id subscriptionRequestId) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  const auto id = _next_subscription_id++;
  _subscriber_list.push_front(TopicSubscriber{id, connection, subscriptionRequestId});
  return id;
}

/**
 * Publish a message to this topic.
 * @param data Message to publish.
 */
void Topic::publish(const std::string& data) {
  nlohmann::json jsonData;
  std::vector<TopicSubscriber> subscribers;

  try {
    jsonData = nlohmann::json::parse(data);
    {
      std::lock_guard<std::mutex> lock(_subscriber_lock);
      subscribers.assign(_subscriber_list.begin(), _subscriber_list.end());
    }

    for (const auto& subscriber : subscribers) {
      if (!hasSubscriber(subscriber.id)) {
        continue;
      }
      auto connection = subscriber.connection.lock();

      if (!connection || connection->isShutdown()) {
        continue;
      }

      if (connection->protocol() == ConnectionProtocol::WEBSOCKET) {
        websocket::Response::sendData(connection,
                                      jsonrpcpp::Response(subscriber.requestId, jsonData).to_json().dump(),
                                      websocket::FrameType::TEXT_FRAME);
      } else if (connection->protocol() == ConnectionProtocol::SSE) {
        sse::Response::sendEvent(connection, jsonData["id"], jsonData["message"]);
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
bool Topic::deleteSubscriber(SubscriptionId id) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  for (auto iterator = _subscriber_list.begin(); iterator != _subscriber_list.end(); ++iterator) {
    if (iterator->id == id) {
      _subscriber_list.erase(iterator);
      return true;
    }
  }
  return false;
}

bool Topic::hasSubscriber(SubscriptionId id) {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  for (const auto& subscriber : _subscriber_list) {
    if (subscriber.id == id) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the number of subscribers on the topic.
 */
std::size_t Topic::getSubscriberCount() {
  std::lock_guard<std::mutex> lock(_subscriber_lock);
  return _subscriber_list.size();
}

} // namespace eventhub
