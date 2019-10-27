#ifndef EVENTHUB_TOPIC_HPP
#define EVENTHUB_TOPIC_HPP

#include "Connection.hpp"
#include <deque>
#include <memory>
#include <mutex>

namespace eventhub {
class Topic {
public:
  Topic(const std::string& topicFilter) { _id = topicFilter; };
  ~Topic(){};

  void addSubscriber(std::shared_ptr<Connection>& conn);
  void publish(const string& data);
  size_t garbageCollect();

private:
  std::string _id;
  uint64_t _n_messages_sent;
  std::deque<std::weak_ptr<Connection>> _subscriber_list;
  std::mutex _subscriber_lock;
};
}; // namespace eventhub

#endif
