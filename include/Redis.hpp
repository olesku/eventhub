#ifndef EVENTHUB_REDIS_HPP
#define EVENTHUB_REDIS_HPP

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sw/redis++/redis++.h>

namespace eventhub {
using namespace std;

using RedisMsgCallback = std::function<void(std::string pattern,
                                            std::string channel,
                                            std::string msg)>;

class Redis {
#define REDIS_PREFIX(key) (_prefix.length() > 0) ? _prefix + "." + key : key
public:
  Redis(const string host, int port = 6379, const string password = "", int poolSize = 5);
  ~Redis(){};

  bool publishMessage(const string topic, const string id, const string payload);
  void psubscribe(const std::string pattern, RedisMsgCallback callback);
  const std::string cacheMessage(const string topic, const string payload);
  void getCache(const string topicPattern, const string since, long limit);
  void consume();
  void resetSubscribers();

  inline void setPrefix(const std::string& prefix) { _prefix = prefix; }

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
