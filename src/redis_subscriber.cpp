
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

      parse(buf, bytes_read);
    }
  }

  void redis_subscriber::parse(const char* data, ssize_t len) {
    static char rest_data[BUFSIZE*2];
    std::string stmt;
    const char *datap;
    static ssize_t rest_data_len = 0;
    ssize_t bytes_processed = 0, stmt_len = 0;

    // TODO: Handle if rest_data + data is more than size of buffer.
    if (rest_data_len > 0) {
      len += rest_data_len;
      memcpy(rest_data+rest_data_len, data, len);
      rest_data[len] = '\0';
      datap = rest_data;
      rest_data_len = 0;
    } else {
      datap = data;
    }

    for (auto i = 0, stmt_len = 1; i < len; i++, stmt_len++) {
      if (datap[i] == '\r' && i+1 < len && datap[i+1] == '\n') {
        i++; // datap+i is now at '\n'.
        stmt_len++;

        //if (bytes_processed > 0) {
          stmt.insert(0, datap+bytes_processed, stmt_len);
          //memcpy(stmt, datap+bytes_processed, stmt_len);       
        //} else {
          //stmt.insert(0, datap, stmt_len);
          //memcpy(stmt, datap, stmt_len);          
       // }

        //stmt[stmt_len] = '\0';

        process_statement(stmt);
        LOG(INFO) << "stmt_len: " << stmt_len;
        bytes_processed += stmt_len;
        stmt_len = 0;
      }
    }

    //LOG(INFO) << "bytes_processed: " << bytes_processed << " len: " << len;

    if (bytes_processed < len) {
      rest_data_len = len-bytes_processed;
      memcpy(rest_data, datap+bytes_processed, rest_data_len);
      rest_data[rest_data_len] = '\0';
      //LOG(INFO) << "We have rest data len: " << rest_data_len << " content: '" << rest_data << "'" ;
    }
  }

  void redis_subscriber::process_statement(const std::string stmt) {
    LOG(INFO) << "process_statement: " << stmt;


  } 
}
