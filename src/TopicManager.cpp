#include "TopicManager.hpp"

#include <ctype.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "Common.hpp"
#include "Topic.hpp"

namespace eventhub {
std::pair<TopicPtr, TopicSubscriberList::iterator> TopicManager::subscribeConnection(ConnectionPtr conn, const std::string& topicFilter, const jsonrpcpp::Id subscriptionRequestId) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  if (!_topic_list.count(topicFilter)) {
    _topic_list.insert(std::make_pair(topicFilter, std::make_unique<Topic>(topicFilter)));
  }

  auto it = _topic_list[topicFilter]->addSubscriber(conn, subscriptionRequestId);

  return std::make_pair(_topic_list[topicFilter], it);
}

void TopicManager::publish(const std::string& topicName, const std::string& data) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  for (auto& topic : _topic_list) {
    if (isFilterMatched(topic.first, topicName)) {
      topic.second->publish(data);
    }
  }
}

void TopicManager::deleteTopic(const std::string& topicFilter) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  if (!_topic_list.count(topicFilter)) {
    LOG(ERROR) << "deleteTopic: Topic " << topicFilter << " does not exist.";
    return;
  }

  auto it = _topic_list.find(topicFilter);
  if (it == _topic_list.end()) {
    LOG(ERROR) << "deleteTopic: " << topicFilter << " does not exist.";
    return;
  }

  _topic_list.erase(it);
}

bool TopicManager::isValidTopicFilter(const std::string& filterName) {
  if (filterName.empty()) {
    return false;
  }

  if (filterName.at(0) == '/') {
    return false;
  }

  if (filterName.find('+') != std::string::npos && filterName.find('#') != std::string::npos) {
    return false;
  }

  for (auto it = filterName.begin(); it != filterName.end(); it++) {
    if (!isalnum(*it) && *it != '-' && *it != '_' && *it != '+' && *it != '#' && *it != '/') {
      return false;
    }

    if (*it == '+' && ((it - 1) == filterName.begin() || (it + 1) == filterName.end() || *(it + 1) != '/' || *(it - 1) != '/')) {
      return false;
    }

    if (*it == '#') {
      if (it == filterName.begin() && (it + 1) == filterName.end()) {
        return true;
      } else if (((it + 1) != filterName.end() || *(it - 1) != '/')) {
        return false;
      }
    }
  }

  return true;
}

// This method assumes topic filter is validated through is_valid_topic_filter.
bool TopicManager::isFilterMatched(const std::string& filterName, const string& topicName) {
  for (auto fnIt = filterName.begin(), tnIt = topicName.begin();
       tnIt != topicName.end(); fnIt++, tnIt++) {
    if (fnIt == filterName.end()) {
      return false;
    }

    if (tnIt + 1 == topicName.end() && fnIt + 1 != filterName.end()) {
      return false;
    }

    if (*fnIt == *tnIt) {
      continue;
    }

    if (*fnIt == '+') {
      for (; tnIt != topicName.end() && *(tnIt + 1) != '/'; tnIt++)
        ;
      if (tnIt == topicName.end())
        return false;
      continue;
    }

    if (*fnIt == '#') {
      break;
    }

    return false;
  }

  return true;
}
} // namespace eventhub
