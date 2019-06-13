
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "common.hpp"
#include "redis_subscriber.hpp"

using namespace std;

namespace eventhub {
  #define BUFSIZE 512

  redis_subscriber::redis_subscriber() {
    DLOG(INFO) << "redis_subscriber destructor.";
    clear_parser_state();
    _connected = false;
  }

  redis_subscriber::~redis_subscriber() {
    disconnect();
  }

  void redis_subscriber::disconnect() {
    if (_connected) close(_fd);
    _connected = false;
  }

  void redis_subscriber::set_callback(std::function<void(const std::string& channel, const std::string& data)> callback) {
    _callback = callback;
  }

  void redis_subscriber::psubscribe(const std::string& pattern) {
    string buf;

    if (!_connected) return;

    buf = "PSUBSCRIBE " + pattern + "\r\n";
    write(_fd, buf.c_str(), buf.length());
  }

  bool redis_subscriber::connect(const std::string& host, const std::string& port) {
    struct addrinfo hints, *res, *rp;
    int ret;

    if (_connected) return true;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
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
      if (ret == 0) break;

      close(_fd);
    }

    if (!_fd || ret != 0) {
      LOG(WARNING) << "redis_subscriber: Could not connect: " << strerror(errno);
      return false;
    }

    DLOG(INFO) << "redis_subscriber: connected to " << host << ":" << port;
    _connected = true;

    thread_main();
    return true;
  }

  void redis_subscriber::thread_main() {
    ssize_t bytes_read = 0;
    char buf[BUFSIZ];

    write(_fd, "SUBSCRIBE test\r\n", strlen("SUBSCRIBE test\r\n"));

    while(_connected) {
      bytes_read = read(_fd, &buf, BUFSIZ);

      // TODO: Implement reconnect.
      if (bytes_read == -1) {
        LOG(INFO) << "redis_subscriber: read() returned -1. Error handling not implemented yet.";
        break;
      }

      buf[bytes_read] == '\0';

      parse(buf);
    }
  }

  void redis_subscriber::clear_parser_state() {
    set_state(READ_RESP);
    _expected_bulk_len = 0;
    _expected_array_len = 0;
  }

  void redis_subscriber::result_array_push(REDIS_DATA_TYPE type, const std::string& data) {
    if (_expected_array_len > 0) {
      LOG(INFO) << "array_push: " << data;
      _result_array.push_back(redis_obj_t{type, data});
    }
  }

  void redis_subscriber::handle_resp(const std::string& data) {
    auto type = data.at(0);
    auto msg = data.substr(1, std::string::npos);

    switch(type) {
      case REDIS_ERROR:
        LOG(INFO) << "Redis error: " << msg;
        clear_parser_state();
        return;
      break;

      case REDIS_BULK_STRING:
        if (data.at(1) == '-' || data.at(1) == '0') {
          // Negative number means there is no elements.
          set_state(READ_RESP);
          return;
        }

        _expected_bulk_len = std::stoi(msg);

        if (_expected_bulk_len > 0) {
          set_state(READ_BULK_STRING);
          LOG(INFO) << "Expect bulk string len: " << _expected_bulk_len;
        }
      break;

      case REDIS_ARRAY:
        _expected_array_len = std::stoi(msg);
        // TODO: Handle NULL arrays.
        if (_expected_array_len > 0) {
          LOG(INFO) << "ARRAY START.";
        }
      break;

      default:
        result_array_push((REDIS_DATA_TYPE)type, msg);
    }
  }

  void redis_subscriber::parse(const char* data) {
    std::string resp_line;

    _parse_buffer.append(data);

    switch(_state) {
      case READ_RESP:
        resp_line = read_until_crlf();
        if (resp_line.empty()) {
          return;
        }

        LOG(INFO) << "CMD: " << resp_line;
        handle_resp(resp_line);
      break;

      case READ_BULK_STRING:
        auto bulk_str = read_bytes(_expected_bulk_len);
        if (!bulk_str.empty()) {
          LOG(INFO) << "Bulk str: " << bulk_str;
          result_array_push(REDIS_BULK_STRING, bulk_str);

          if (!_parse_buffer.empty()) {
            parse("");
          }
        }
      break;
    }

    LOG(INFO) << "expected_array_len: " << _expected_array_len << " actual array len: " << _result_array.size();
    if (_expected_array_len > 0 && _result_array.size() == _expected_array_len) {
      _expected_array_len = 0;

      if (_result_array.size() == 3 && _result_array.at(0).data.compare("pmessage") == 0) {
        handle_pmessage(_result_array);
        _result_array.clear();
        clear_parser_state();
      }
    }
  }

  const std::string redis_subscriber::read_bytes(size_t n_bytes) {
    if (_parse_buffer.length() >= n_bytes) {
      auto bulk = _parse_buffer.substr(0, n_bytes);

      _parse_buffer.replace(0, n_bytes, "");

      if (_parse_buffer.substr(0, 2).compare("\r\n") == 0) {
        _parse_buffer.replace(0, 2, "");
        set_state(READ_RESP);
        return bulk;
      } else {
        // TODO: Handle error if we don't have a \r\n here.
        LOG(INFO) << "Error: bulk string does not end with \r\n";
        set_state(READ_RESP);
        _parse_buffer.clear();
      }
    }

    return "";
  }

  const std::string redis_subscriber::read_until_crlf() {
    LOG(INFO) << "parse_buffer: " << "'" << _parse_buffer << "'";
    for (auto i = 0, data_len = 1; i < _parse_buffer.length(); i++, data_len++) {
      if (_parse_buffer.at(i) == '\r' && i+1 < _parse_buffer.length() && _parse_buffer.at(i+1) == '\n') {
        i++; // datap+i is now at '\n'.
        data_len--; // remove the '\r' from the command.

        auto data = _parse_buffer.substr(0, data_len);
        _parse_buffer.replace(0, data_len+2, ""); // +2 because of \r\n.
        return data;
      }
    }

    return "";
  }

  void redis_subscriber::handle_pmessage(redis_array_t& data) {
    auto& channel = data.at(1).data;
    auto& msg = data.at(2).data;

    _callback(channel, msg);
    LOG(INFO) << "handle_pmessage -> channel: " << channel << " message: " << msg;
  }
}
