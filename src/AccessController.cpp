#include <spdlog/logger.h>
#include <stdint.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <exception>
#include <initializer_list>
#include <memory>

#include "AccessController.hpp"
#include "Config.hpp"
#include "TopicManager.hpp"
#include "Logger.hpp"
#include "jwt/impl/jwt.ipp"
#include "jwt/json/json.hpp"
#include "jwt/parameters.hpp"

namespace eventhub {

#define REQUIRE_TOKEN_LOADED(x) \
  if (!_token_loaded)           \
    return false;

#define BYPASS_AUTH_IF_DISABLED(x)    \
  if (config().get<bool>("disable_auth")) \
    return true;


// authenticate loads a a JWT token and extracts ACL's for publish and subscribe.
bool AccessController::authenticate(const std::string& jwtToken, const std::string& secret) {
  BYPASS_AUTH_IF_DISABLED();

  try {
    _token = jwt::decode(jwtToken, jwt::params::algorithms({"hs256"}), jwt::params::secret(secret));

    auto& payload = _token.payload();

    if (payload.has_claim("write")) {
      for (auto filter : payload.get_claim_value<std::vector<std::string>>("write")) {
        if (TopicManager::isValidTopicOrFilter(filter)) {
          _publish_acl.push_back(filter);
        }
      }
    }

    if (payload.has_claim("read")) {
      for (auto filter : payload.get_claim_value<std::vector<std::string>>("read")) {
        if (TopicManager::isValidTopicOrFilter(filter)) {
          _subscribe_acl.push_back(filter);
        }
      }
    }

    if (payload.has_claim("sub")) {
      _subject = payload.get_claim_value<std::string>("sub");
    }

    if ((_subscribe_acl.size() + _publish_acl.size()) == 0) {
      throw std::invalid_argument("No publish or subscribe ACL defined in JWT token.");
    }

    if (payload.has_claim("sub")) {
      _subject = payload.get_claim_value<std::string>("sub");
    }

    if (payload.has_claim("rlimit")) {
      auto payload_json = payload.create_json_obj();
      if (payload_json.is_object() && payload_json["rlimit"].is_array()) {
        _rlimit.loadFromJSON(payload_json["rlimit"]);
      }
    }
  } catch (std::exception& e) {
    LOG->trace("Error in AccessController: ", e.what());
    return false;
  }

  _token_loaded = true;

  return true;
}

const std::string& AccessController::subject() {
  return _subject;
}

bool AccessController::isAuthenticated() {
  BYPASS_AUTH_IF_DISABLED();
  REQUIRE_TOKEN_LOADED();
  return true;
}

// allowPublish checks if the loaded token is allowed to publish to topic.
bool AccessController::allowPublish(const std::string& topic) {
  BYPASS_AUTH_IF_DISABLED();
  REQUIRE_TOKEN_LOADED();

  for (auto& filter : _publish_acl) {
    if (TopicManager::isFilterMatched(filter, topic)) {
      return true;
    }
  }

  return false;
}

// allowPublish checks if the loaded token is allowed to subscribe to topic.
bool AccessController::allowSubscribe(const std::string& topic) {
  BYPASS_AUTH_IF_DISABLED();
  REQUIRE_TOKEN_LOADED();

  for (auto& filter : _subscribe_acl) {
    if (TopicManager::isFilterMatched(filter, topic)) {
      return true;
    }
  }

  return false;
}

bool AccessController::allowCreateToken(const std::string& path) {
  BYPASS_AUTH_IF_DISABLED();
  REQUIRE_TOKEN_LOADED();
  // Not implemented yet.
  return true;
}


bool RateLimitConfig::loadFromJSON(const nlohmann::json::array_t& config) {
  for (const auto& rlimit: config) {
    if (!rlimit.is_object())
      continue;

    try {
      auto topic = rlimit["topic"].get<std::string>();
      auto interval = rlimit["interval"].get<unsigned long>();
      auto max = rlimit["max"].get<unsigned long>();

      _limitConfigs.push_back(rlimit_config_t{topic, interval, max});
    } catch (...) {
      continue;
    }
  }

  return true;
}

// Returns limits for given topic if there are any.
// If no limits is defined we throw NoRateLimitForTopic exception.
const rlimit_config_t RateLimitConfig::getRateLimitForTopic(const std::string& topic) {
  rlimit_config_t rlimit;
  bool found = false;
  size_t matchedPatternLen = 0;

  // Exit early if no limits is present in token.
  if (_limitConfigs.empty())
    throw(NoRateLimitForTopic{});

  /*
    Check if we have any limits defined for topic or matching pattern.
  */
  for (const auto& limit : _limitConfigs) {
    // Exact match has highest precedence, use that if we have one.
    if (limit.topic.compare(topic) == 0) {
      rlimit = limit;
      found = true;
      break;
    }

    // We check the length here to chose the closest matching pattern if there are more than one.
    if (TopicManager::isFilterMatched(limit.topic, topic) && limit.topic.length() > matchedPatternLen) {
        rlimit = limit;
        found = true;
        matchedPatternLen = limit.topic.length();
    }
  }

  if (!found)
      throw(NoRateLimitForTopic{});

  return rlimit;
}

} // namespace eventhub
