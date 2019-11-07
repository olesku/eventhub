#ifndef INCLUDE_TOPICMANAGER_HPP_
#define INCLUDE_TOPICMANAGER_HPP_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "Common.hpp"
#include "Connection.hpp"
#include "Topic.hpp"

namespace eventhub {
using TopicList = std::unordered_map<std::string, TopicPtr>;

class TopicManager {
public:
  std::pair<TopicPtr, TopicSubscriberList::iterator> subscribeConnection(ConnectionPtr conn, const std::string& topicFilter, const jsonrpcpp::Id subscriptionRequestId);
  void publish(const std::string& topicName, const std::string& data);
  void deleteTopic(const std::string& topicFilter);

  static bool isValidTopicFilter(const std::string& filterName);
  static bool isFilterMatched(const std::string& filterName, const string& topicName);

private:
  TopicList _topic_list;
  std::mutex _topic_list_lock;
};
} // namespace eventhub

#endif // INCLUDE_TOPICMANAGER_HPP_
