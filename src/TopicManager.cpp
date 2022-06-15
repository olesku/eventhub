#include <ctype.h>
#include <spdlog/logger.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <list>

#include "TopicManager.hpp"
#include "Topic.hpp"
#include "Logger.hpp"

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

  // A filter must contain either + or #.
  auto hashBangPos = filterName.find('#');
  if (filterName.find('+') == std::string::npos && hashBangPos == std::string::npos) {
    return false;
  }

  // A # must always be at the end.
  if (hashBangPos != std::string::npos && (hashBangPos + 1) != filterName.length()) {
    return false;
  }

  for (auto it = filterName.begin(); it != filterName.end(); it++) {
    if (!isalnum(*it) && *it != '-' && *it != '_' && *it != '+' && *it != '#' && *it != '/') {
      return false;
    }

    if (*it == '+' && (it + 1) != filterName.end() && ((*(it + 1) != '/' || (*(it - 1) != '/' && (it) != filterName.begin())))) {
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
bool TopicManager::isFilterMatched(const std::string& filterName, const std::string& topicName) {
  // Loop through filterName and topicName one character at the time.
  for (auto fnIt = filterName.begin(), tnIt = topicName.begin();
       tnIt != topicName.end(); fnIt++, tnIt++) {
    // We reached the end of our filter without a match.
    if (fnIt == filterName.end()) {
      return false;
    }

    // We have reached the end of the topic.
    if (tnIt + 1 == topicName.end() && fnIt + 1 != filterName.end()) {
      if (*fnIt != *tnIt) {
        return false;
      }

      // Match the root topic in addition to every subtopic
      // when we have a match-all (#) on that path.
      // Example: topic/foo/# should also match topic/foo.
      if ((fnIt + 2 != filterName.end() && fnIt + 3 == filterName.end()) &&
          *(fnIt + 1) == '/' && *(fnIt + 2) == '#') {
        return true;
      }

      return false;
    }

    // Filter character at current pos matches topicName character.
    if (*fnIt == *tnIt) {
      continue;
    }

    // If we hit a + in the filter we should increment the pos of
    // the topicName until we match a '/'.
    if (*fnIt == '+') {
      for (; tnIt != topicName.end() && *(tnIt + 1) != '/'; tnIt++)
        ;
      // If we reached the end of the topicName before we found a '/'
      // it means we don't have a match.
      if (tnIt == topicName.end() && fnIt + 1 == filterName.end())
        return true;
      else if (tnIt == topicName.end())
        return false;
      continue;
    }

    // We matched up until now and the filter ends with a '#'
    // which means we should match.
    if (*fnIt == '#' && fnIt + 1 == filterName.end()) {
      break;
    }

    return false;
  }

  // We have a match.
  return true;
}
} // namespace eventhub
