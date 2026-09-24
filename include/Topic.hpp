#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <stddef.h>
#include <string>
#include <vector>

#include "Connection.hpp"
#include "jsonrpc/jsonrpcpp.hpp"

namespace eventhub {

using TopicPtr       = std::shared_ptr<class Topic>;
using SubscriptionId = std::uint64_t;

struct TopicSubscriber {
  SubscriptionId id;
  ConnectionWeakPtr connection;
  jsonrpcpp::Id requestId;
};

using TopicSubscriberList = std::list<TopicSubscriber>;

class Topic final {
public:
  explicit Topic(const std::string& topicFilter) { _id = topicFilter; }
  ~Topic();

  SubscriptionId addSubscriber(ConnectionPtr connection, const jsonrpcpp::Id subscriptionRequestId);
  bool deleteSubscriber(SubscriptionId id);
  bool hasSubscriber(SubscriptionId id);
  void publish(const std::string& data);
  std::size_t getSubscriberCount();

private:
  std::string _id;
  TopicSubscriberList _subscriber_list;
  std::mutex _subscriber_lock;
  SubscriptionId _next_subscription_id{1};
};

}; // namespace eventhub
