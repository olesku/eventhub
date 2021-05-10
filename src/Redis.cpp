#include "Redis.hpp"

#include <chrono>
#include <deque>
#include <fmt/format.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <stdexcept>

#include "Common.hpp"
#include "Config.hpp"
#include "TopicManager.hpp"
#include "Util.hpp"
#include "Server.hpp"
#include "jwt/json/json.hpp"

namespace eventhub {

using namespace std;

Redis::Redis(Config &cfg) : EventhubBase(cfg) {
  sw::redis::ConnectionOptions connOpts;
  sw::redis::ConnectionPoolOptions poolOpts;

  connOpts.keep_alive     = true;
  connOpts.host           = config().get<std::string>("redis_host");
  connOpts.port           = config().get<int>("redis_port");
  connOpts.socket_timeout = std::chrono::seconds(5);

  _prefix = config().get<std::string>("redis_prefix");
  const auto& password = config().get<std::string>("redis_password");

  if (password.length() > 0) {
    connOpts.password = password;
  }

  poolOpts.connection_lifetime = std::chrono::milliseconds(0);
  poolOpts.size                = config().get<int>("redis_pool_size");
  poolOpts.wait_timeout        = std::chrono::seconds(5);

  _redisInstance   = std::make_unique<sw::redis::Redis>(connOpts, poolOpts);
  _redisSubscriber = nullptr;
}

// Publish a message.
void Redis::publishMessage(const string topic, const string id, const string payload, const string sender) {
  std::lock_guard<std::mutex> lock(_publish_mtx);
  nlohmann::json j;

  j["topic"]   = topic;
  j["id"]      = id;
  j["message"] = payload;

  if (!sender.empty()) {
    j["sender"]  = sender;
  }

  auto jsonData = j.dump();
  _redisInstance->publish(redis_prefix(topic), jsonData);
}

// Returns a unique ID in the format <timeSinceEpoch>-<sequenceNo>.
const std::string Redis::_getNextCacheId(long long timestamp) {
  const auto idKey = redis_prefix(fmt::format("id:{}", timestamp));

  auto id = _redisInstance->incr(idKey);
  _redisInstance->expire(idKey, 1);
  id--; // Start at 0.

  return fmt::format("{}-{}", timestamp, id);
}

// Add a message to the cache.
const std::string Redis::cacheMessage(const string topic, const string payload, const string sender, long long timestamp, unsigned int ttl) {
  if (timestamp == 0) {
    timestamp = Util::getTimeSinceEpoch();
  }

  auto cacheId = _getNextCacheId(timestamp);

  // Do not cache message if cache functionality is disabled.
  if (!config().get<bool>("enable_cache")) {
    return cacheId;
  }

  if (ttl == 0) {
    ttl = config().get<int>("default_cache_ttl");
  }

  auto expireAt = Util::getTimeSinceEpoch() + (ttl * 1000);
  auto zKey     = CacheItemMeta{cacheId, sender, expireAt}.toStr();

  _redisInstance->hset(REDIS_CACHE_DATA_PATH(topic), cacheId, payload);
  _redisInstance->zadd(REDIS_CACHE_SCORE_PATH(topic), zKey, timestamp);

  _incrTopicPubCount(topic);

  return cacheId;
}

// GetCache returns all matching cached messages for topics matching topicPattern
// @param since List all messages since Unix timestamp or message ID
// @param limit Limit resultset to at most @limit elements.
size_t Redis::getCacheSince(const string topicPattern, long long since, long long limit, bool isPattern, nlohmann::json& result) {
  std::vector<std::string> topics;
  result = nlohmann::json::array();

  // If cache is not enabled simply return an empty set.
  if (!config().get<bool>("enable_cache")) {
    return 0;
  }

  auto now = Util::getTimeSinceEpoch();

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

  for (const auto& topic : topics) {
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
    std::unordered_map<std::string, std::string> senderIdMap;

    for (auto zKey : zcacheKeys) {
      try {
        auto p = CacheItemMeta{zKey};

        if (p.expireAt() >= now) {
          cacheKeys.push_back(p.id());
          senderIdMap[p.id()] = p.sender();
        }
      } catch (...) {
        continue;
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
      LOG->error("Mismatch between cache score set and cache data set for topic {}.", topic);
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

      if (!senderIdMap[cacheKeys[i]].empty()) {
        j["sender"]  = senderIdMap[cacheKeys[i]];
      }

      result.push_back(j);
    }
  }

  return result.size();
}

std::pair<long long, long long> _splitIdAndSeq(const string cacheId) {
  auto hyphenPos = cacheId.find_first_of('-');
  if (hyphenPos == std::string::npos || hyphenPos == 0) {
    throw invalid_argument("Invalid cache id.");
  }

  auto tsStr     = cacheId.substr(0, hyphenPos);
  auto timestamp = std::stoull(tsStr, nullptr, 10);
  auto seqStr    = cacheId.substr(hyphenPos + 1, std::string::npos);
  auto seq       = std::stoull(seqStr, nullptr, 10);

  return {timestamp, seq};
}

// Get cached messages after a given message ID.
size_t Redis::getCacheSinceId(const string topicPattern, const string sinceId, long long limit, bool isPattern, nlohmann::json& result) {
  result = nlohmann::json::array();

  // If cache is not enabled simply return an empty set.
  if (!config().get<bool>("enable_cache")) {
    return 0;
  }

  try {
    long long timestamp, seqNo;
    std::tie(timestamp, seqNo) = _splitIdAndSeq(sinceId);

    if (timestamp == 0) {
      return 0;
    }

    // Then request the cache since that timestamp.
    if (getCacheSince(topicPattern, timestamp, limit, isPattern, result) == 0) {
      return 0;
    }

    // The loop below removes entries with equal timestamp and same or lower sequence id.
    bool sinceIdFound = false;
    auto it           = result.begin();

    while (it != result.end()) {
      long long elm_Timestamp, elm_seqNo;
      const auto elm = *it;

      // Validate the cache object and return empty result if an invalid
      // object is found.
      if (!elm.is_object() || !elm.contains("id") || !elm.contains("topic") || !elm.contains("message")) {
        LOG->error("Found invalid cache object in getCacheSinceId.\n  topicPattern: {} sinceId: {} isPattern: {} limit: {}\n cacheElement: {}\n",
                   topicPattern, sinceId, isPattern, limit, elm.dump());

        result.clear();
        return 0;
      }

      std::tie(elm_Timestamp, elm_seqNo) = _splitIdAndSeq(elm["id"]);

      // sinceId is surpassed and not found.
      // Return empty result.
      if (elm_Timestamp > timestamp || (elm_Timestamp == timestamp && elm_seqNo > seqNo)) {
        if (!sinceIdFound) {
          result.clear();
          return 0;
        }
      }

      if (elm_Timestamp == timestamp && elm_seqNo <= seqNo) {
        // We only want to return results if we find sinceId in the cache.
        if (elm_seqNo == seqNo) {
          sinceIdFound = true;
        }

        it = result.erase(it);
      } else {
        it++;
      }
    }

    // sinceId was not found in the cache, return empty result.
    if (!sinceIdFound) {
      result.clear();
      return 0;
    }
  } catch (...) {
    result.clear();
    return 0;
  }

  return result.size();
}

// Delete expired items from the cache.
size_t Redis::purgeExpiredCacheItems() {
  std::vector<std::string> allTopics;
  std::vector<std::pair<std::string, std::string>> expiredItems;
  auto now = Util::getTimeSinceEpoch();

  _redisInstance->hkeys(redis_prefix("pub_count"), std::back_inserter(allTopics));

  for (auto topic : allTopics) {
    std::vector<std::string> cacheKeys;
    _redisInstance->zrange(REDIS_CACHE_SCORE_PATH(topic), 0, -1, std::back_inserter(cacheKeys));

    if (!cacheKeys.empty()) {
      for (auto key : cacheKeys) {
        try {
          auto p = CacheItemMeta{key};

          if (p.expireAt()< now) {
            expiredItems.push_back({topic, key});
          }
        } catch(...) {
          continue;
        }
      }
    } else {
      // Remove topic from pub_count if it has no more cached
      // elements left.
      _redisInstance->hdel(redis_prefix("pub_count"), topic);
    }
  }

  for (auto exp : expiredItems) {
    auto topic = exp.first;
    auto key   = exp.second;
    auto p     = CacheItemMeta{key};

    _redisInstance->zrem(REDIS_CACHE_SCORE_PATH(topic), key);
    _redisInstance->hdel(REDIS_CACHE_DATA_PATH(topic), p.id());
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

  _redisSubscriber->psubscribe(redis_prefix(pattern));
}

void Redis::consume() {
  _redisSubscriber->consume();
}

// _incrTopicPubCount increase the counter of published messages to topicName
// in our Redis stats HSET.
void Redis::_incrTopicPubCount(const string& topicName) {
  _redisInstance->hincrby(redis_prefix("pub_count"), topicName, 1);
}

// _getTopicsSeen Look up in our pubcount HSET in redis and return
// all topics we have received events on that matches topicPattern.
std::vector<std::string> Redis::_getTopicsSeen(const string& topicPattern) {
  std::vector<std::string> allTopics;
  std::vector<std::string> matchingTopics;

  _redisInstance->hkeys(redis_prefix("pub_count"), std::back_inserter(allTopics));

  for (auto& topic : allTopics) {
    if (TopicManager::isFilterMatched(topicPattern, topic)) {
      matchingTopics.push_back(topic);
    }
  }

  return matchingTopics;
}

CacheItemMeta::CacheItemMeta(const std::string& id, const std::string& sender, long expireAt) :
  _id(id), _sender(sender), _expireAt(expireAt) {}

CacheItemMeta::CacheItemMeta(const std::string& metaStr) {
  std::string expireAtStr;
  unsigned int j = 0;

  for (unsigned int i = 0; i < metaStr.length(); i++) {
    if (metaStr[i] == ':') {
      j++;
      continue;
    }

    if (j == 0) _id += metaStr[i];
    else if (j == 1) expireAtStr += metaStr[i];
    else if (j == 2) _sender += metaStr[i];
  }

  if (j < 1) {
    throw std::runtime_error("Invalid CacheItemMetaStr '" + metaStr + "'");
  }

  _expireAt = std::stol(expireAtStr, nullptr, 10);
}

const std::string CacheItemMeta::toStr() {
  if (_sender.empty()) {
    return fmt::format("{}:{}", _id, _expireAt);
  }

  return fmt::format("{}:{}:{}", _id, _expireAt, _sender);
}

} // namespace eventhub
