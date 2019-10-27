#ifndef EVENTHUB_REDIS_HPP
#define EVENTHUB_REDIS_HPP

#include "message.hpp"
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <sw/redis++/redis++.h>

namespace eventhub {
using namespace std;

using RedisMsgCallback = std::function<void (std::string pattern,
                                                  std::string channel,
                                                  std::string msg)>;

class Redis {
#define REDIS_PREFIX(key) (_prefix.length() > 0) ? _prefix + "." + key : key
public:
  Redis(const string host, int port=6379, const string password="", int pool_size=5);
  ~Redis(){};

  bool PublishMessage(const string topic, const string id, const string payload);
  void Psubscribe(const std::string pattern, RedisMsgCallback callback);
  const std::string CacheMessage(const string topic, const string payload);
  void GetCache(const string topicPattern, const string since, long limit);
  void Consume();
  void ResetSubscribers();

  inline void SetPrefix(const std::string& prefix) { _prefix = prefix; }
private:


  std::unique_ptr<sw::redis::Redis> _redisInstance;
  std::unique_ptr<sw::redis::Subscriber> _redisSubscriber;
  std::string _prefix;
  std::mutex _publish_mtx;
  void _incrTopicPubCount(const string& topicName);
  vector<string> _getTopicsSeen(const string& topicPattern);
};

} // namespace eventhub

#endif
