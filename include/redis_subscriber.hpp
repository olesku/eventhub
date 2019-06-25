#ifndef EVENTHUB_REDIS_SUBSCRIBER_HPP
#define EVENTHUB_REDIS_SUBSCRIBER_HPP

#include <functional>
#include <map>
#include <mutex>
#include <string.h>
#include <sys/epoll.h>
#include <thread>
#include <vector>

namespace eventhub {
using redis_pmessage_callback_t = std::function<void(const std::string& pattern, const std::string& channel, const std::string& data)>;
class RedisSubscriber {
  typedef enum {
    READ_RESP        = 0,
    READ_BULK_STRING = 1
  } REDIS_PARSER_STATE;

  typedef enum {
    REDIS_SIMPLE_STRING = '+',
    REDIS_ERROR         = '-',
    REDIS_INTEGER       = ':',
    REDIS_BULK_STRING   = '$',
    REDIS_ARRAY         = '*'
  } REDIS_DATA_TYPE;

  typedef struct {
    REDIS_DATA_TYPE type;
    std::string data;
    size_t len;
  } redis_obj_t;

  typedef std::vector<redis_obj_t> redis_array_t;

private:
  std::thread _thread;
  std::mutex _mtx;
  int _fd, _epoll_fd;

  std::string _host;
  std::string _port;
  bool _connected, _stop;

  REDIS_PARSER_STATE _state;

  size_t _expected_bulk_len;
  size_t _expected_array_len;

  std::string _parse_buffer;
  std::string _bulk_buffer;

  redis_array_t _result_array;

  std::map<std::string, redis_pmessage_callback_t> _pmessage_callbacks;

  void _clearParserState();
  void _threadMain();
  void _handleResp(const std::string& data);
  void _handlePmessage();
  void _handleMessage();
  void _reconnect();
  const std::string _readUntilCRLF();
  const std::string _readBytes(size_t bulk_size);
  inline void _set_parser_state(REDIS_PARSER_STATE state) { _state = state; }
  inline REDIS_PARSER_STATE _get_parser_state() { return _state; }
  void _parse(const char* data);

public:
  RedisSubscriber();
  ~RedisSubscriber();

  bool connect(const std::string& host, const std::string& port);
  void disconnect();
  inline bool isConnected() { return _connected; }
  void psubscribe(const std::string& pattern, redis_pmessage_callback_t callback);
};
} // namespace eventhub

#endif
