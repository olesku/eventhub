#ifndef EVENTHUB_TOPIC_MANAGER_HPP
#define EVENTHUB_TOPIC_MANAGER_HPP

#include "common.hpp"
#include "connection.hpp"
#include "topic.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace eventhub {
class TopicManager {
  typedef std::unordered_map<std::string, std::unique_ptr<Topic>> topic_list_t;

public:
  void subscribeConnection(std::shared_ptr<Connection>& conn, const std::string& topic_filter);
  void publish(const std::string& topic_name, const std::string& data);
  void garbageCollect();

  static bool isValidTopicFilter(const std::string& filter_name);
  static bool isFilterMatched(const std::string& filter_name, const string& topic_name);
  static const std::string uriDecode(const std::string& str);

private:
  topic_list_t _topic_list;
  std::mutex _topic_list_lock;
};
} // namespace eventhub

#endif
