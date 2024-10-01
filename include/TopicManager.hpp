#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "Forward.hpp"
#include "Common.hpp"
#include "Connection.hpp"
#include "Topic.hpp"
#include "jsonrpc/jsonrpcpp.hpp"

namespace eventhub {

using TopicList = std::unordered_map<std::string, TopicPtr>;

class TopicManager final {
public:
  std::pair<TopicPtr, TopicSubscriberList::iterator> subscribeConnection(ConnectionPtr conn, const std::string& topicFilter, const jsonrpcpp::Id subscriptionRequestId);
  void publish(std::string_view topicName, const std::string& data);
  void deleteTopic(const std::string& topicFilter);

  static bool isValidTopic(std::string_view topicName);
  static bool isValidTopicFilter(std::string_view filterName);
  static bool isValidTopicOrFilter(std::string_view topic);
  static bool isFilterMatched(std::string_view filterName, std::string_view topicName);

private:
  TopicList _topic_list;
  std::mutex _topic_list_lock;
};
} // namespace eventhub
