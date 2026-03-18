#include <ctype.h>
#include <spdlog/logger.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <list>
#include <string_view>
#include <vector>

#include "TopicManager.hpp"
#include "Topic.hpp"
#include "Logger.hpp"

namespace eventhub {
namespace {
std::vector<std::string_view> splitLevels(const std::string& input) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;

  for (std::size_t i = 0; i <= input.size(); ++i) {
    if (i == input.size() || input[i] == '/') {
      parts.emplace_back(input.data() + start, i - start);
      start = i + 1;
    }
  }

  return parts;
}
} // namespace
/*
* Subscribe a client to a topic.
* @param conn Client to subscribe.
* @param topicFilter Topic or filter name.
* @param subscriptionRequestId JSONRPC ID for request.
*/
std::pair<TopicPtr, TopicSubscriberList::iterator> TopicManager::subscribeConnection(ConnectionPtr conn, const std::string& topicFilter, const jsonrpcpp::Id subscriptionRequestId) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  auto it = _topic_list.find(topicFilter);
  if (it == _topic_list.end()) {
    it = _topic_list.emplace(topicFilter, std::make_shared<Topic>(topicFilter)).first;
  }

  auto subIt = it->second->addSubscriber(conn, subscriptionRequestId);

  return std::make_pair(it->second, subIt);
}

/*
* Publish to a topic.
* @param topicName topic to publish to.
* @param data message to publish.
*/
void TopicManager::publish(const std::string& topicName, const std::string& data) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  for (auto& topic : _topic_list) {
    if (isFilterMatched(topic.first, topicName)) {
      topic.second->publish(data);
    }
  }
}

/*
* Delete a topic.
* @param topicFilter topic to delete.
*/
void TopicManager::deleteTopic(const std::string& topicFilter) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  auto it = _topic_list.find(topicFilter);
  if (it == _topic_list.end()) {
    LOG->error("deleteTopic: {} does not exist.", topicFilter);
    return;
  }

  _topic_list.erase(it);
}

/*
* Check if a topic name is valid.
* Rules: Cannot be empty, start or end with '/', and only contain [a-zA-Z0-9-_].
* @param topicName topic name to validate.
* @returns true if valid, false otherwise.
*/
bool TopicManager::isValidTopic(const std::string& topicName) {
  if (topicName.empty()) {
    return false;
  }

  if (topicName.at(0) == '/') {
    return false;
  }

  if (topicName.find('+') != std::string::npos || topicName.find('#') != std::string::npos) {
    return false;
  }

  for (auto it = topicName.begin(); it != topicName.end(); it++) {
    if (!isalnum(*it) && *it != '-' && *it != '_' && *it != '/') {
      return false;
    }

    if (it + 1 == topicName.end() && *it == '/') {
      return false;
    }
  }

  return true;
}

/*
* Check if a topic filter is valid.
* Rules: Cannot be empty, start or end with '/', only contain [a-zA-Z0-9-_], must contain # or + but not at the same time.
* @param filterName filter to validate.
* @returns true if valid, false otherwise.
*/
bool TopicManager::isValidTopicFilter(const std::string& filterName) {
  if (filterName.empty()) {
    return false;
  }

  // Cannot start with a /.
  if (filterName.at(0) == '/') {
    return false;
  }

  bool hasWildcard = false;
  for (auto it = filterName.begin(); it != filterName.end(); it++) {
    if (!isalnum(*it) && *it != '-' && *it != '_' && *it != '+' && *it != '#' && *it != '/') {
      return false;
    }

    if (*it == '+') {
      hasWildcard = true;
      const bool prevOk = (it == filterName.begin()) || (*(it - 1) == '/');
      const bool nextOk = ((it + 1) == filterName.end()) || (*(it + 1) == '/');
      if (!prevOk || !nextOk) {
        return false;
      }
    } else if (*it == '#') {
      hasWildcard = true;
      const bool prevOk = (it == filterName.begin()) || (*(it - 1) == '/');
      if (!prevOk || (it + 1) != filterName.end()) {
        return false;
      }
    }
  }

  return hasWildcard;
}

/*
* Helper function to check if input is a valid topic or topic filter.
* @param topic input.
*/
bool TopicManager::isValidTopicOrFilter(const std::string& topic) {
  if (isValidTopic(topic) || isValidTopicFilter(topic)) {
    return true;
  }

  return false;
}

/*
* Check if a filter matches a topic.
* @param filterName filter to use.
* @param topicName topic name to validate.
* @returns true if it matches, false otherwise.
*/
bool TopicManager::isFilterMatched(const std::string& filterName, const std::string& topicName) {
  const auto filterLevels = splitLevels(filterName);
  const auto topicLevels  = splitLevels(topicName);

  std::size_t topicIndex = 0;
  for (std::size_t filterIndex = 0; filterIndex < filterLevels.size(); ++filterIndex, ++topicIndex) {
    const auto filterLevel = filterLevels[filterIndex];

    if (filterLevel == "#") {
      return true;
    }

    if (topicIndex >= topicLevels.size()) {
      return false;
    }

    if (filterLevel == "+") {
      continue;
    }

    if (filterLevel != topicLevels[topicIndex]) {
      return false;
    }
  }

  return topicIndex == topicLevels.size();
}
} // namespace eventhub
