#include "TopicManager.hpp"
#include "Topic.hpp"
#include <ctype.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace eventhub {
void TopicManager::subscribeConnection(ConnectionPtr conn, const std::string& topicFilter) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  if (!_topic_list.count(topicFilter)) {
    DLOG(INFO) << "Created topic " << topicFilter;
    _topic_list.insert(std::make_pair(topicFilter, std::unique_ptr<Topic>(new Topic(topicFilter))));
  }

  _topic_list[topicFilter]->addSubscriber(conn);
}

void TopicManager::publish(const std::string& topicName, const std::string& data) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  for (auto& topic : _topic_list) {
    if (isFilterMatched(topic.first, topicName)) {
      topic.second->publish(data);
    }
  }
}

void TopicManager::garbageCollect() {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  for (auto it = _topic_list.begin(); it != _topic_list.end();) {
    auto n = it->second->garbageCollect();

    if (n == 0) {
      DLOG(INFO) << it->first << " has no more connections, removing.";
      it = _topic_list.erase(it);
    } else {
      it++;
    }
  }
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
    if (fnIt == filterName.end())
      return false;

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
