#include "topic_manager.hpp"
#include "topic.hpp"
#include <ctype.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace eventhub {
void TopicManager::subscribeConnection(std::shared_ptr<Connection>& conn, const std::string& topic_filter) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  if (!_topic_list.count(topic_filter)) {
    DLOG(INFO) << "Created topic " << topic_filter;
    _topic_list.insert(std::make_pair(topic_filter, std::unique_ptr<Topic>(new Topic(topic_filter))));
  }

  _topic_list[topic_filter]->addSubscriber(conn);
}

void TopicManager::publish(const std::string& topic_name, const std::string& data) {
  std::lock_guard<std::mutex> lock(_topic_list_lock);

  for (auto& topic : _topic_list) {
    if (isFilterMatched(topic.first, topic_name)) {
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

bool TopicManager::isValidTopicFilter(const std::string& filter_name) {
  if (filter_name.empty()) {
    return false;
  }

  if (filter_name.at(0) == '/') {
    return false;
  }

  if (filter_name.find('+') != std::string::npos && filter_name.find('#') != std::string::npos) {
    return false;
  }

  for (auto it = filter_name.begin(); it != filter_name.end(); it++) {
    if (!isalnum(*it) && *it != '-' && *it != '_' && *it != '+' && *it != '#' && *it != '/') {
      return false;
    }

    if (*it == '+' && ((it - 1) == filter_name.begin() || (it + 1) == filter_name.end() || *(it + 1) != '/' || *(it - 1) != '/')) {
      return false;
    }

    if (*it == '#') {
      if (it == filter_name.begin() && (it + 1) == filter_name.end()) {
        return true;
      } else if (((it + 1) != filter_name.end() || *(it - 1) != '/')) {
        return false;
      }
    }
  }

  return true;
}

// This method assumes topic filter is validated through is_valid_topic_filter.
bool TopicManager::isFilterMatched(const std::string& filter_name, const string& topic_name) {
  for (auto fn_it = filter_name.begin(), tn_it = topic_name.begin();
       tn_it != topic_name.end(); fn_it++, tn_it++) {
    if (fn_it == filter_name.end())
      return false;

    if (*fn_it == *tn_it) {
      continue;
    }

    if (*fn_it == '+') {
      for (; tn_it != topic_name.end() && *(tn_it + 1) != '/'; tn_it++)
        ;
      if (tn_it == topic_name.end())
        return false;
      continue;
    }

    if (*fn_it == '#') {
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
