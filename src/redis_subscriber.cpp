
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
    set_state(READ_COMMAND);
    _expected_bulk_len = 0;
    _connected = false;
  }

  redis_subscriber::~redis_subscriber() {
    disconnect();
  }

  void redis_subscriber::disconnect() {
    if (_connected) close(_fd);
    _connected = false;
  }

  void redis_subscriber::set_callback(std::function<void(const std::string& topic, const std::string& data)> callback) {
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
    char buf[BUFSIZE];

    write(_fd, "SUBSCRIBE test\r\n", strlen("SUBSCRIBE test\r\n"));

    while(_connected) {
      bytes_read = read(_fd, &buf, BUFSIZE);

      // TODO: Implement reconnect.
      if (bytes_read == -1) {
        LOG(INFO) << "redis_subscriber: read() returned -1. Error handling not implemented yet.";
        break;
      }

      buf[bytes_read] == '\0';

      parse(buf);
    }
  }

  void redis_subscriber::parse(const char* data) {
    std::string cmd;

    _parse_buffer.append(data);

    switch(_state) {
      case READ_COMMAND:
        cmd = read_until_crlf();
        if (cmd.empty()) {
          return;
        }

        LOG(INFO) << "CMD: " << cmd;
        switch(cmd.at(0)) {
          // Simple string.
          case '+':

          break;

          // Error.
          case '-':

          break;

          // Integer.
          case ':':

          break;

          // Bulk string.
          case '$':
            _expected_bulk_len = std::stoi(cmd.substr(1, std::string::npos));

            if (_expected_bulk_len > 0) {
              set_state(READ_BULK_STRING);
              LOG(INFO) << "Expect bulk string len: " << _expected_bulk_len;
            }
          break;

          // Array.
          case '*':

          break;

          case '\n':
            LOG(INFO) << "End of command";
          break;

        }
      break;

      case READ_BULK_STRING:
        auto bulk_str = read_bytes(_expected_bulk_len);
        if (!bulk_str.empty()) {
          LOG(INFO) << "Bulk str: " << bulk_str;

          if (!_parse_buffer.empty()) {
            parse("");
          }
        }
      break;
    }
  }

  const std::string redis_subscriber::read_bytes(size_t n_bytes) {
    if (_parse_buffer.length() >= n_bytes) {
      auto bulk = _parse_buffer.substr(0, n_bytes);

      _parse_buffer.replace(0, n_bytes, "");

      if (_parse_buffer.substr(0, 2).compare("\r\n") == 0) {
        _parse_buffer.replace(0, 2, "");
        set_state(READ_COMMAND);
        return bulk;
      } else {
        // TODO: Handle error if we don't have a \r\n here.
        LOG(INFO) << "Error: bulk string does not end with \r\n";
        set_state(READ_COMMAND);
        _parse_buffer.clear();
      }
    }

    return "";
  }

  const std::string redis_subscriber::read_until_crlf() {
    LOG(INFO) << "parse_buffer: " << "'" << _parse_buffer << "'";
    for (auto i = 0, cmd_len = 1; i < _parse_buffer.length(); i++, cmd_len++) {
      if (_parse_buffer.at(i) == '\r' && i+1 < _parse_buffer.length() && _parse_buffer.at(i+1) == '\n') {

        // \r\n - End of command.
        if (i == 0) {
          _parse_buffer.replace(0, 2, "");
          return "\n";
        }

        i++; // datap+i is now at '\n'.
        cmd_len--; // remove the '\r' from the command.

        auto cmd = _parse_buffer.substr(0, cmd_len);
        _parse_buffer.replace(0, cmd_len+2, ""); // +2 because of \r\n.
        return cmd;
      }
    }

    return "";
  }
}