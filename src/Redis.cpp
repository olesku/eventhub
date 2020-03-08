#include "Redis.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

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

// Returns a unique ID.
long long Redis::_getNextCacheId(const std::string topic) {
  auto id = _redisInstance->hincrby(REDIS_CACHE_DATA_PATH(topic), "_idCnt", 1);
  return id;
}

// CacheMessage cache a message.
const std::string Redis::cacheMessage(const string topic, const string payload, double timestamp) {
  stringstream cacheId;
  cacheId << _getNextCacheId(topic);

  if (timestamp == 0) {
    timestamp = Util::getTimeSinceEpoch();
  }

  _redisInstance->hset(REDIS_CACHE_DATA_PATH(topic), cacheId.str(), payload);
  _redisInstance->zadd(REDIS_CACHE_SCORE_PATH(topic), cacheId.str(), timestamp);
  _incrTopicPubCount(topic);

  return cacheId.str();
}

// GetCache returns all matching cached messages for topics matching topicPattern
// @param since List all messages since Unix timestamp or message ID
// @param limit Limit resultset to at most @limit elements.
size_t Redis::getCache(const string topicPattern, const string since, size_t limit, bool isPattern, nlohmann::json& result) {
  std::vector<std::string> topics;
  result = nlohmann::json::array();

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
    std::vector<std::string> cacheKeys;
    // TODO: Pass in since parameter (instead of 0).
    _redisInstance->zrangebyscore(REDIS_CACHE_SCORE_PATH(topic), sw::redis::BoundedInterval<double>(0, Util::getTimeSinceEpoch(), sw::redis::BoundType::CLOSED),
            std::back_inserter(cacheKeys));

    // No cache for this topic, go to the next one.
    if (cacheKeys.size() < 1) {
      continue;
    }

     std::vector<sw::redis::OptionalString> cacheItems;
    _redisInstance->hmget(REDIS_CACHE_DATA_PATH(topic), cacheKeys.begin(), cacheKeys.end(), std::back_inserter(cacheItems));

    // If there is a mismatch between the length of the ZSET (timestamps) and the HSET (data)
    // for a given topic, something is messed up. In this case we perform purge of the cache for that topic.
    if (cacheItems.size() != cacheKeys.size()) {
      LOG(ERROR) << "ERROR: Missmatch between cache score set and cache data set for topic " << topic;
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

      LOG(INFO) << topic << ":cache[" << cacheKeys[i] << "] = " << cacheItems[i].value();
    }
  }

  return result.size();
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
