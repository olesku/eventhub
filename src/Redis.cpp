#include "Redis.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <deque>

#include "Common.hpp"
#include "Config.hpp"
#include "Util.hpp"
#include "TopicManager.hpp"
#include "jwt/json/json.hpp"

namespace eventhub {

using namespace std;

Redis::Redis(const string host, int port, const string password, int poolSize) {
  sw::redis::ConnectionOptions connOpts;
  sw::redis::ConnectionPoolOptions poolOpts;

  connOpts.keep_alive     = true;
  connOpts.host           = host;
  connOpts.port           = port;
  connOpts.socket_timeout = std::chrono::seconds(5);

  if (password.length() > 0) {
    connOpts.password = password;
  }

  poolOpts.connection_lifetime = std::chrono::milliseconds(0);
  poolOpts.size                = poolSize;
  poolOpts.wait_timeout        = std::chrono::seconds(5);

  _redisInstance   = std::make_unique<sw::redis::Redis>(connOpts, poolOpts);
  _redisSubscriber = nullptr;
}

// Publish a message.
void Redis::publishMessage(const string topic, const string id, const string payload) {
  std::lock_guard<std::mutex> lock(_publish_mtx);
  nlohmann::json j;

  j["topic"]   = topic;
  j["id"]      = id;
  j["message"] = payload;

  auto jsonData = j.dump();
  _redisInstance->publish(REDIS_PREFIX(topic), jsonData);
}

// Returns a unique ID in the format <timeSinceEpoch>-<sequenceNo>.
const std::string Redis::_getNextCacheId(const std::string topic) {
  stringstream timestamp_ms;
  timestamp_ms << (Util::getTimeSinceEpoch() / 1000);

  auto id = _redisInstance->incr(REDIS_PREFIX(topic + ":" + timestamp_ms.str()));
  _redisInstance->expire(REDIS_PREFIX(topic + ":" + timestamp_ms.str()), 1);

  id--; // Start at 0.
  timestamp_ms << "-" << id;

  return timestamp_ms.str();
}

// Takes in a string in the form of <HSET-ID>-<ExpireAt> and returns
// the id and expire time as a pair.
const std::pair<std::string, int64_t> Redis::_parseIdAndExpireAt(const std::string& input) {
  auto delimPos = input.find(':');
  auto id = input.substr(0, delimPos);
  auto expireAtStr = input.substr(delimPos+1, std::string::npos);
  return {id, std::stol(expireAtStr, nullptr, 10)};
}

// Add a message to the cache.
const std::string Redis::cacheMessage(const string topic, const string payload, long long timestamp, unsigned int ttl) {
  auto cacheId = _getNextCacheId(topic);

  if (timestamp == 0) {
    timestamp = Util::getTimeSinceEpoch();
  }

  if (ttl == 0) {
    ttl = Config.getInt("DEFAULT_CACHE_TTL");
  }

  auto expireAt = (Util::getTimeSinceEpoch() / 1000) + ttl;
  stringstream zKey;
  zKey << cacheId << ":" << expireAt;

  _redisInstance->hset(REDIS_CACHE_DATA_PATH(topic), cacheId, payload);
  _redisInstance->zadd(REDIS_CACHE_SCORE_PATH(topic), zKey.str(), timestamp);

  _incrTopicPubCount(topic);

  return cacheId;
}

// GetCache returns all matching cached messages for topics matching topicPattern
// @param since List all messages since Unix timestamp or message ID
// @param limit Limit resultset to at most @limit elements.
size_t Redis::getCache(const string topicPattern, long long since, long long limit, bool isPattern, nlohmann::json& result) {
  std::vector<std::string> topics;
  result = nlohmann::json::array();
  auto now = Util::getTimeSinceEpoch() / 1000;

  // Look up all matching topics in redis we get a request for a topic pattern
  // and request the eventlog for each of them.
  if (isPattern) {
    for (auto& topic : _getTopicsSeen(topicPattern)) {
       topics.push_back(topic);
    }
  }
  // If it is a single topic then only look up eventlog for that.
  else {
    topics.push_back(topicPattern);
  }

  for (const auto topic : topics) {
    // zcacheKeys is in format <hset-id>:<expireAtTimestamp>
    std::deque<std::string> zcacheKeys;

    // ZREVRANGEBYSCORE <path> +inf <since> limit 0 <limit>
    // We use a front_inserter here and a back_inserter when retrieving the actual data from the HSET.
    // We do this to reverse the order of returned items. We are interested in the newest cache items from <since> to <now>
    // up to <limit> items, but we want the order to be from oldest to newest while ZREVRANGEBYSCORE gives us the opposite.
    _redisInstance->zrevrangebyscore(REDIS_CACHE_SCORE_PATH(topic), sw::redis::LeftBoundedInterval<double>(since, sw::redis::BoundType::RIGHT_OPEN),
            sw::redis::LimitOptions{0, limit}, std::front_inserter(zcacheKeys));

    // Only look up keys that is not expired.
    std::vector<std::string> cacheKeys;

    for (auto zKey : zcacheKeys) {
      auto p  = _parseIdAndExpireAt(zKey);
      if (p.second >= now) {
        cacheKeys.push_back(p.first);
      }
    }

    // No cache for this topic, go to the next one.
    if (cacheKeys.size() < 1) {
      continue;
    }

    std::vector<sw::redis::OptionalString> cacheItems;
    _redisInstance->hmget(REDIS_CACHE_DATA_PATH(topic), cacheKeys.begin(), cacheKeys.end(), std::back_inserter(cacheItems));

    // If there is a mismatch between the length of the ZSET (timestamps) and the HSET (data)
    // for a given topic, something is messed up. In this case we perform purge of the cache for that topic.
    if (cacheItems.size() != cacheKeys.size()) {
      LOG->error("Missmatch between cache score set and cache data set for topic {}.", topic);
      _redisInstance->del({REDIS_CACHE_DATA_PATH(topic), REDIS_CACHE_SCORE_PATH(topic)});
      continue;
    }

    for (unsigned int i = 0; i < cacheItems.size(); i++) {
      // Key returned from ZSET does not exist in the HSET anymore.
      // continue on to the next key.
      if (cacheItems[i].value().empty()) {
        continue;
      }

      nlohmann::json j;
      j["id"]      = cacheKeys[i];
      j["topic"]   = topic;
      j["message"] = cacheItems[i].value();
      result.push_back(j);
    }
  }

  return result.size();
}

// Delete expired items from the cache.
size_t Redis::purgeExpiredCacheItems() {
  std::vector<std::string> allTopics, cacheKeys;
  std::vector<std::pair<std::string,std::string>> expiredItems;
  auto now = Util::getTimeSinceEpoch() / 1000;

  _redisInstance->hkeys(REDIS_PREFIX("pub_count"), std::back_inserter(allTopics));

  for (auto topic : allTopics) {
    _redisInstance->zrange(REDIS_CACHE_SCORE_PATH(topic), 0, -1, std::back_inserter(cacheKeys));

    for (auto key : cacheKeys) {
      auto p = _parseIdAndExpireAt(key);

      if (p.second < now) {
        expiredItems.push_back({topic, key});
      }
    }
  }

  for (auto exp : expiredItems) {
    auto topic = exp.first;
    auto key = exp.second;
    auto p = _parseIdAndExpireAt(key);

    _redisInstance->zrem(REDIS_CACHE_SCORE_PATH(topic), key);
    _redisInstance->hdel(REDIS_CACHE_DATA_PATH(topic), p.first);
  }

  return expiredItems.size();
}

/*
  According to redis++ documentation the library should handle reconnects itself.
  However, this proves not to be true for subscriber connections.
  So we have to trigger a reset and setup subscribers when we lose the server connection.
*/
void Redis::resetSubscribers() {
  if (_redisSubscriber != nullptr) {
    _redisSubscriber.reset();
  }

  _redisSubscriber = nullptr;
}

// Register a subscriber callback.
void Redis::psubscribe(const std::string pattern, RedisMsgCallback callback) {
  if (_redisSubscriber == nullptr) {
    _redisSubscriber = std::make_unique<sw::redis::Subscriber>(_redisInstance->subscriber());
  }

  _redisSubscriber->on_pmessage([=](string pattern, string topic, string msg) {
    string actualTopic;

    if (_prefix.length() > 0) {
      actualTopic = topic.substr(_prefix.length() + 1, string::npos);
    } else {
      actualTopic = topic;
    }

    callback(pattern, actualTopic, msg);
  });

  _redisSubscriber->psubscribe(REDIS_PREFIX(pattern));
}

void Redis::consume() {
  _redisSubscriber->consume();
}

// _incrTopicPubCount increase the counter of published messages to topicName
// in our Redis stats HSET.
void Redis::_incrTopicPubCount(const string& topicName) {
  _redisInstance->hincrby(REDIS_PREFIX("pub_count"), topicName, 1);
}

// _getTopicsSeen Look up in our pubcount HSET in redis and return
// all topics we have received events on that matches topicPattern.
std::vector<std::string> Redis::_getTopicsSeen(const string& topicPattern) {
  std::vector<std::string> allTopics;
  std::vector<std::string> matchingTopics;

  _redisInstance->hkeys(REDIS_PREFIX("pub_count"), std::back_inserter(allTopics));

  for (auto& topic : allTopics) {
    if (TopicManager::isFilterMatched(topicPattern, topic)) {
      matchingTopics.push_back(topic);
    }
  }

  return matchingTopics;
}

} // namespace eventhub
