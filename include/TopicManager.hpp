#ifndef EVENTHUB_TOPIC_MANAGER_HPP
#define EVENTHUB_TOPIC_MANAGER_HPP

#include "Common.hpp"
//#include "Connection.hpp"
#include "Topic.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

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

#endif
