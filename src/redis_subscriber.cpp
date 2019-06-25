
#include "redis_subscriber.hpp"
#include "common.hpp"
#include <errno.h>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>

using namespace std;

namespace eventhub {
RedisSubscriber::RedisSubscriber() {
  _clearParserState();
  _connected = false;
  _stop      = false;
}

RedisSubscriber::~RedisSubscriber() {
  disconnect();
  _stop = true;

  if (_thread.joinable())
    _thread.join();
}

void RedisSubscriber::disconnect() {
  if (!_connected)
    return;

  _connected = false;
  shutdown(_fd, SHUT_RDWR);
  close(_fd);
}

void RedisSubscriber::_reconnect() {
  disconnect();

  while (!_stop) {
    LOG(INFO) << "Reconnecting to Redis " << _host << ":" << _port;
    connect(_host, _port);
    if (_connected) {
      for (auto& p : _pmessage_callbacks) {
        LOG(INFO) << "Resubscribing to " << p.first;
        psubscribe(p.first, p.second);
      }
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  }
}

void RedisSubscriber::psubscribe(const std::string& pattern, redis_pmessage_callback_t callback) {
  std::lock_guard<std::mutex> lock(_mtx);
  stringstream buf;

  // TODO: Check if we already are subscribed and implement some exception handling.

  buf << "*2\r\n";
  buf << "$10\r\n";
  buf << "psubscribe\r\n";
  buf << "$" << pattern.length() << "\r\n";
  buf << pattern << "\r\n";

  _pmessage_callbacks[pattern] = callback;

  if (!_connected)
    return;
  ::write(_fd, buf.str().c_str(), buf.str().length());
  DLOG(INFO) << buf.str();
}

bool RedisSubscriber::connect(const std::string& host, const std::string& port) {
  struct addrinfo hints, *res, *rp;
  int ret;

  if (_connected)
    return true;

  _host = host;
  _port = port;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags    = 0;
  hints.ai_protocol = 0;

  // Resolve hostname.
  ret = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
  if (ret != 0) {
    LOG(WARNING) << "redis_subscriber: getaddrinfo: " << gai_strerror(ret);
    return false;
  }

  // Loop through results until we get a successful connection.
  for (rp = res; rp != NULL; rp = rp->ai_next) {
    _fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

    if (_fd == -1) {
      continue;
    }

    ret = ::connect(_fd, rp->ai_addr, rp->ai_addrlen);
    if (ret == 0)
      break;

    close(_fd);
  }

  if (!_fd || ret != 0) {
    LOG(WARNING) << "redis_subscriber: Could not connect: " << strerror(errno);
    return false;
  }

  DLOG(INFO) << "redis_subscriber: connected to " << host << ":" << port;
  _connected = true;

  if (!_thread.joinable())
    _thread = std::thread(&RedisSubscriber::_threadMain, this);

  return true;
}

void RedisSubscriber::_threadMain() {
  ssize_t bytes_read = 0;
  char buf[BUFSIZ];

  LOG(INFO) << "Starting redis threadMain";

  while (!_stop && _connected) {
    bytes_read = read(_fd, &buf, BUFSIZ);

    // TODO: Implement reconnect.
    if (bytes_read < 1) {
      _reconnect();
      continue;
    }

    buf[bytes_read] = '\0';

    DLOG(INFO) << std::endl
               << "read_buffer:" << std::endl
               << "'" << buf << "'" << std::endl;

    _parse(buf);
  }

  LOG(INFO) << "Stopping redis threadMain";
}

void RedisSubscriber::_clearParserState() {
  _set_parser_state(READ_RESP);
  _expected_bulk_len  = 0;
  _expected_array_len = 0;
}

void RedisSubscriber::_handleResp(const std::string& data) {
  auto type = data.at(0);
  auto msg  = data.substr(1, std::string::npos);

  switch (type) {
    case REDIS_ERROR:
      LOG(INFO) << "Redis error: " << msg;
      _clearParserState();
      return;
      break;

    case REDIS_BULK_STRING:
      if (data.at(1) == '-' || data.at(1) == '0') {
        // Negative number means there is no elements.
        _set_parser_state(READ_RESP);
        return;
      }

      try {
        _expected_bulk_len = std::stoi(msg);
      } catch (std::exception& e) {
        _expected_bulk_len = 0;
      }

      if (_expected_bulk_len > 0) {
        _set_parser_state(READ_BULK_STRING);
      }

      _parse(NULL);
      break;

    case REDIS_ARRAY:
      try {
        _expected_array_len = std::stoi(msg);
      } catch (std::exception& e) {
        _expected_array_len = 0;
      }

      if (_expected_array_len > 0) {
        // TODO: Handle NULL arrays.
      }
      break;

    default:
      if (_expected_array_len > 0) {
        _result_array.push_back(redis_obj_t{(REDIS_DATA_TYPE)type, msg});
      }
  }
}

void RedisSubscriber::_parse(const char* data) {
  if (data != NULL) {
    _parse_buffer.append(data);
  }

  // Handle RESP commands.
  while (_get_parser_state() == READ_RESP) {
    auto resp_line = _readUntilCRLF();

    if (!resp_line.empty()) {
      DLOG(INFO) << "RESP: " << resp_line;
      _handleResp(resp_line);
      continue;
    }

    break;
  }

  // Handle bulk strings.
  if (_get_parser_state() == READ_BULK_STRING) {
    auto bulk_str = _readBytes(_expected_bulk_len);

    if (!bulk_str.empty()) {
      DLOG(INFO) << "Bulk str: " << bulk_str;
      if (_expected_array_len > 0) {
        _result_array.push_back(redis_obj_t{(REDIS_DATA_TYPE)REDIS_BULK_STRING, bulk_str});
      }

      _set_parser_state(READ_RESP);

      if (!_parse_buffer.empty()) {
        _parse(NULL);
      }
    }
  }

  // Handle arrays.
  if (_expected_array_len > 0 && _result_array.size() == _expected_array_len) {
    DLOG(INFO) << "Array(" << _expected_array_len << ")";
    _expected_array_len = 0;

    if (_result_array.size() == 3 || _result_array.size() == 4) {
      auto& cmd = _result_array.at(0).data;

      if (cmd.compare("message") == 0) {
        _handleMessage();
      } else if (cmd.compare("pmessage") == 0) {
        _handlePmessage();
      }
    }

    _result_array.clear();
    _clearParserState();
  }
}

const std::string RedisSubscriber::_readBytes(size_t n_bytes) {
  if (_parse_buffer.length() >= n_bytes) {
    auto bulk = _parse_buffer.substr(0, n_bytes);
    _parse_buffer.erase(0, n_bytes);

    return bulk;
  }

  return "";
}

const std::string RedisSubscriber::_readUntilCRLF() {
  for (auto i = 0, data_len = 1; i < _parse_buffer.length(); i++, data_len++) {
    if (_parse_buffer.at(i) == '\r' && i + 1 < _parse_buffer.length() && _parse_buffer.at(i + 1) == '\n') {
      data_len--; // Remove the \r.
      auto data = _parse_buffer.substr(0, data_len);
      _parse_buffer.erase(0, data_len + 2); // +2 because of \r\n.
      return data;
    }
  }

  return "";
}

void RedisSubscriber::_handlePmessage() {
  auto& pattern                                                       = _result_array.at(1).data;
  auto& channel                                                       = _result_array.at(2).data;
  auto& payload                                                       = _result_array.at(3).data;
  std::map<std::string, redis_pmessage_callback_t>::iterator callback = _pmessage_callbacks.find(pattern);

  if (callback != _pmessage_callbacks.end()) {
    DLOG(INFO) << "Calling callback for " << callback->first;
    callback->second(pattern, channel, payload);
  }
}

void RedisSubscriber::_handleMessage() {
}
} // namespace eventhub
