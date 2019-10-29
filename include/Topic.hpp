#ifndef EVENTHUB_TOPIC_HPP
#define EVENTHUB_TOPIC_HPP

#include <list>
#include <memory>
#include <mutex>
#include <string>

namespace eventhub {
class Connection;
using TopicSubscriberList = std::list<std::weak_ptr<Connection>>;

class Topic {
public:
  Topic(const std::string& topicFilter) { _id = topicFilter; };
  ~Topic();

  TopicSubscriberList::iterator addSubscriber(std::shared_ptr<Connection> conn);
  void deleteSubscriberByIterator(TopicSubscriberList::iterator it);
  void publish(const std::string& data);
  inline size_t getSubscriberCount() { return _subscriber_list.size(); }

private:
  std::string _id;
  uint64_t _n_messages_sent;
  TopicSubscriberList _subscriber_list;
  std::mutex _subscriber_lock;
};

using TopicPtr = std::shared_ptr<Topic>;
}; // namespace eventhub
#endif
