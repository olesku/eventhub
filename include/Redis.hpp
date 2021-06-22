#ifndef INCLUDE_REDIS_HPP_
#define INCLUDE_REDIS_HPP_

#include <sw/redis++/redis++.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <fmt/format.h>

#include "Forward.hpp"
#include "EventhubBase.hpp"
#include "jwt/json/json.hpp"

namespace eventhub {

using namespace std;

using RedisMsgCallback = std::function<void(std::string pattern,
                                            std::string channel,
                                            std::string msg)>;

class CacheItemMeta {
public:
  explicit CacheItemMeta(const std::string& id, long expireAt, const std::string& origin);

  explicit CacheItemMeta(const std::string& metaStr);

  ~CacheItemMeta() {};

  const std::string& id() { return _id; }
  const std::string& origin() { return _origin; }
  long expireAt() const { return _expireAt; }
  const std::string toStr();

private:
  std::string _id;
  long int _expireAt = 0;
  std::string _origin;
};

class Redis final : public EventhubBase {
#define REDIS_PREFIX(key) std::string((_prefix.length() > 0) ? _prefix + ":" + key : key)
#define REDIS_CACHE_SCORE_PATH(key) std::string(REDIS_PREFIX(key) + ":scores")
#define REDIS_CACHE_DATA_PATH(key) std::string(REDIS_PREFIX(key) + ":cache")

public:
  explicit Redis(Config &cfg);
  ~Redis() {}

  void publishMessage(const string topic, const string id, const string payload, const string origin="");
  void psubscribe(const std::string pattern, RedisMsgCallback callback);
  const std::string cacheMessage(const string topic, const string payload, const string origin, long long timestamp = 0, unsigned int ttl = 0);
  size_t getCacheSince(const string topicPattern, long long since, long long limit, bool isPattern, nlohmann::json& result);
  size_t getCacheSinceId(const string topicPattern, const string sinceId, long long limit, bool isPattern, nlohmann::json& result);
  size_t purgeExpiredCacheItems();
  void consume();
  void resetSubscribers();
  sw::redis::Redis* getRedisInstance() { return _redisInstance.get(); }

  void _incrTopicPubCount(const string& topicName);
  vector<string> _getTopicsSeen(const string& topicPattern);
  const std::string _getNextCacheId(long long timestamp);

private:
  std::unique_ptr<sw::redis::Redis> _redisInstance;
  std::unique_ptr<sw::redis::Subscriber> _redisSubscriber;
  std::string _prefix;
  std::mutex _publish_mtx;
};

} // namespace eventhub

#endif // INCLUDE_REDIS_HPP_
