#include "Redis.hpp"
#include "TopicManager.hpp"
#include "jwt/json/json.hpp"
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

namespace eventhub {

using XStreamAttrs = std::vector<std::pair<std::string, std::string>>;
using XStreamItem  = std::pair<std::string, XStreamAttrs>;
using XItemStream  = std::vector<XStreamItem>;
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

  j["topic"]    = topic;
  j["cacheId"]  = id;
  j["message"]  = payload;

  auto jsonData = j.dump();
  _redisInstance->publish(REDIS_PREFIX(topic), jsonData);
}

// CacheMessage caches a message for topic and payload into
// the corresponding Redis XSTREAM.
const std::string Redis::cacheMessage(const string topic, const string payload) {
  XStreamAttrs attrs = {{topic, payload}};
  auto id            = _redisInstance->xadd(REDIS_PREFIX(topic), "*", attrs.begin(), attrs.end());
  _incrTopicPubCount(topic);
  return id;
}

// GetCache returns all matching cached messages for topics matching topicPattern
// @param since List all messages since Unix timestamp or message ID
// @param limit Limit resultset to at most @limit elements.
size_t Redis::getCache(const string topicPattern, const string since, size_t limit, nlohmann::json& result) {
  auto topicsSeen = _getTopicsSeen(topicPattern);
  std::unordered_map<std::string, std::string> keys;
  result = nlohmann::json::array();

  for (auto& topic : topicsSeen) {
    keys.insert(pair<std::string, std::string>(REDIS_PREFIX(topic), since));
  }

  std::unordered_map<std::string, XItemStream> redisResult;

  if (keys.size() > 0) {
    _redisInstance->xread(keys.begin(), keys.end(), limit, std::inserter(redisResult, redisResult.end()));
  }

  if (redisResult.size() > 0) {
    for (auto& streamID : redisResult) {
      for (auto& msgID : streamID.second) {
        for (auto& keyVals : msgID.second) {
          /*
          auto& streamName = streamID.first;
          auto& payloadID  = msgID.first;
          auto& topicName  = keyVals.first;
          auto& payload    = keyVals.second;
          */
          nlohmann::json j;
          j["cacheId"]  = msgID.first;
          j["topic"]    = keyVals.first;
          j["message"]  = keyVals.second;

          result.push_back(j);
        }
      }
    }
  }

  return redisResult.size();
}

/*
  Accordding to redis++ documentation the library should handle reconnects itself.
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
