#ifndef EVENTHUB_TOPIC_HPP
#define EVENTHUB_TOPIC_HPP

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include "jsonrpc/jsonrpcpp.hpp"
#include "Connection.hpp"

namespace eventhub {

using TopicPtr = std::shared_ptr<class Topic>;
using TopicSubscriberList = std::list<std::pair<ConnectionWeakPtr, jsonrpcpp::Id>>;

class Topic {
public:
  Topic(const std::string& topicFilter) { _id = topicFilter; };
  ~Topic();

  TopicSubscriberList::iterator addSubscriber(ConnectionPtr conn, const jsonrpcpp::Id subscriptionRequestId);
  void deleteSubscriberByIterator(TopicSubscriberList::iterator it);
  void publish(const std::string& data);
  inline size_t getSubscriberCount() { return _subscriber_list.size(); }

private:
  std::string _id;
  uint64_t _n_messages_sent;
  TopicSubscriberList _subscriber_list;
  std::mutex _subscriber_lock;
};

}; // namespace eventhub
#endif