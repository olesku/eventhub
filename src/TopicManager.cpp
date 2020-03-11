#include "TopicManager.hpp"

#include <ctype.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "Common.hpp"
#include "Topic.hpp"

namespace eventhub {
/*
* Subscribe a client to a topic.
* @param conn Client to subscribe.
* @param topicFilter Topic or filter name.
* @param subscriptionRequestId JSONRPC ID for request.
*/
std::pair<TopicPtr, TopicSubscriberList::iterator> TopicManager::subscribeConnection(ConnectionPtr conn, const std::string& topicFilter, const jsonrpcpp::Id subscriptionRequestId) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  if (!_topic_list.count(topicFilter)) {
    _topic_list.insert(std::make_pair(topicFilter, std::make_unique<Topic>(topicFilter)));
  }

  auto it = _topic_list[topicFilter]->addSubscriber(conn, subscriptionRequestId);

  return std::make_pair(_topic_list[topicFilter], it);
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
    spdlog::error("deleteTopic: {} does not exist.", topicFilter);
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

    if (it+1 == topicName.end() && *it == '/') {
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

  // Cannot contain + and # at the same time.
  if (filterName.find('+') != std::string::npos && filterName.find('#') != std::string::npos) {
    return false;
  }

  // Must contain either + or #.
  if (filterName.find('+') == std::string::npos && filterName.find('#') == std::string::npos) {
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
