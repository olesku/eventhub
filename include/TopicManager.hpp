#ifndef EVENTHUB_TOPIC_MANAGER_HPP
#define EVENTHUB_TOPIC_MANAGER_HPP

#include "Common.hpp"
#include "Connection.hpp"
#include "Topic.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace eventhub {
class TopicManager {
  typedef std::unordered_map<std::string, std::unique_ptr<Topic>> topic_list_t;

public:
  void subscribeConnection(ConnectionPtr conn, const std::string& topicFilter);
  void publish(const std::string& topicName, const std::string& data);
  void garbageCollect();

  static bool isValidTopicFilter(const std::string& filterName);
  static bool isFilterMatched(const std::string& filterName, const string& topicName);

private:
  topic_list_t _topic_list;
  std::mutex _topic_list_lock;
};
} // namespace eventhub

#endif
