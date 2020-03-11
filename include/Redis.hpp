#ifndef INCLUDE_REDIS_HPP_
#define INCLUDE_REDIS_HPP_

#include <sw/redis++/redis++.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "jwt/json/json.hpp"

namespace eventhub {
using namespace std;

using RedisMsgCallback = std::function<void(std::string pattern,
                                            std::string channel,
                                            std::string msg)>;

class Redis {
#define CONST_24HRS_MS (86400 * 1000)
#define REDIS_PREFIX(key) std::string((_prefix.length() > 0) ? _prefix + ":" + key : key)
#define REDIS_CACHE_SCORE_PATH(key) std::string(REDIS_PREFIX(key) + ":scores")
#define REDIS_CACHE_DATA_PATH(key) std::string(REDIS_PREFIX(key) + ":cache")

public:
  explicit Redis(const string host, int port = 6379, const string password = "", int poolSize = 5);
  ~Redis() {}

  void publishMessage(const string topic, const string id, const string payload);
  void psubscribe(const std::string pattern, RedisMsgCallback callback);
  const std::string cacheMessage(const string topic, const string payload, long long timestamp=0, unsigned int ttl=0);
  size_t getCache(const string topicPattern, long long since, long long limit, bool isPattern, nlohmann::json& result);
  size_t purgeExpiredCacheItems();
  void consume();
  void resetSubscribers();
  inline void setPrefix(const std::string& prefix) { _prefix = prefix; }
  inline sw::redis::Redis* getRedisInstance() { return _redisInstance.get(); }

  void _incrTopicPubCount(const string& topicName);
  vector<string> _getTopicsSeen(const string& topicPattern);
  const std::string _getNextCacheId(const std::string topic);
  const std::pair<std::string, int64_t> _parseIdAndExpireAt(const std::string& input);

private:
  std::unique_ptr<sw::redis::Redis> _redisInstance;
  std::unique_ptr<sw::redis::Subscriber> _redisSubscriber;
  std::string _prefix;
  std::mutex _publish_mtx;
};

} // namespace eventhub

#endif // INCLUDE_REDIS_HPP_
