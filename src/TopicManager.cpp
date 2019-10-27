#include "TopicManager.hpp"
#include "Topic.hpp"
#include <ctype.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace eventhub {
void TopicManager::subscribeConnection(std::shared_ptr<Connection>& conn, const std::string& topicFilter) {
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

const std::string TopicManager::uriDecode(const std::string& str) {
  std::ostringstream unescaped;
  for (std::string::const_iterator i = str.begin(), n = str.end(); i != n; ++i) {
    std::string::value_type c = (*i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '*' || c == '/' || c == '+') {
      unescaped << c;
    } else if (c == '%') {
      // throw error if string is invalid and doesn't have 2 char after,
      // or if it has non-hex chars here (courtesy GitHub @scinart)
      if (i + 2 >= n || !isxdigit(*(i + 1)) || !isxdigit(*(i + 2))) {
        DLOG(INFO) << "urlDecode: Invalid percent-encoding";
        return "";
      }

      // decode a URL-encoded ASCII character, e.g. %40 => &
      char ch1        = *(i + 1);
      char ch2        = *(i + 2);
      int hex1        = (isdigit(ch1) ? (ch1 - '0') : (toupper(ch1) - 'A' + 10));
      int hex2        = (isdigit(ch2) ? (ch2 - '0') : (toupper(ch2) - 'A' + 10));
      int decodedChar = (hex1 << 4) + hex2;
      unescaped << (char)decodedChar;
      i += 2;
    } else {
      std::ostringstream msg;
      DLOG(INFO) << "urlDecode: Unexpected character in string: "
                 << (int)c << " (" << c << ")";
      return "";
    }
  }

  return unescaped.str();
}
} // namespace eventhub
