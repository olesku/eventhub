#pragma once

#include <stddef.h>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "Connection.hpp"
#include "jsonrpc/jsonrpcpp.hpp"

namespace eventhub {

using TopicPtr            = std::shared_ptr<class Topic>;
using TopicSubscriberList = std::list<std::pair<ConnectionWeakPtr, jsonrpcpp::Id>>;

class Topic final {
public:
  explicit Topic(const std::string& topicFilter) { _id = topicFilter; }
  ~Topic();

  TopicSubscriberList::iterator addSubscriber(ConnectionPtr conn, const jsonrpcpp::Id subscriptionRequestId);
  void deleteSubscriberByIterator(TopicSubscriberList::iterator it);
  void publish(const std::string& data);
  std::size_t getSubscriberCount();

private:
  std::string _id;
  TopicSubscriberList _subscriber_list;
  std::mutex _subscriber_lock;
};

}; // namespace eventhub


