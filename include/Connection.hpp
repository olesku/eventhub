#pragma once

#include <netinet/in.h>
#include <stdint.h>
#ifdef __linux__
#include <sys/epoll.h>
#else
#include "EpollWrapper.hpp"
#endif
#include <sys/socket.h>
#include <ctime>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EventhubBase.hpp"
#include "AccessController.hpp"
#include "Common.hpp"
#include "http/Parser.hpp"
#include "jsonrpc/jsonrpcpp.hpp"
#include "websocket/Parser.hpp"
#include "websocket/Types.hpp"
#include "Forward.hpp"

namespace eventhub {
using ConnectionPtr          = std::shared_ptr<Connection>;
using ConnectionWeakPtr      = std::weak_ptr<Connection>;
using ConnectionListIterator = std::list<ConnectionPtr>::iterator;

enum class ConnectionState {
  HTTP,
  WEBSOCKET,
  SSE
};

struct TopicSubscription {
  std::shared_ptr<Topic> topic;
  std::list<std::pair<ConnectionWeakPtr, jsonrpcpp::Id>>::iterator topicListIterator;
  jsonrpcpp::Id rpcSubscriptionRequestId;
};

class Connection : public EventhubBase, public std::enable_shared_from_this<Connection> {
public:
  Connection(int fd, struct sockaddr_in* csin, Worker* worker, Config& cfg);
  virtual ~Connection();

  void write(const std::string& data);
  virtual void read();
  virtual ssize_t flushSendBuffer();

  int addToEpoll(uint32_t epollEvents);
  int removeFromEpoll();

  ConnectionState setState(ConnectionState newState);
  ConnectionState getState();
  AccessController& getAccessController();
  void assignConnectionListIterator(std::list<ConnectionPtr>::iterator connectionIterator);
  ConnectionListIterator getConnectionListIterator();
  ConnectionPtr getSharedPtr();
  const std::string getIP();

  void subscribe(const std::string& topicPattern, const jsonrpcpp::Id subscriptionRequestId);
  bool unsubscribe(const std::string& topicPattern);
  unsigned int unsubscribeAll();
  std::vector<std::string> listSubscriptions();

  void onHTTPRequest(http::ParserCallback callback);
  void onWebsocketRequest(websocket::ParserCallback callback);

  void shutdownAfterFlush();
  void shutdown();
  bool isShutdown() { return _is_shutdown; }

protected:
  int _fd;
  struct sockaddr_in _csin;
  Worker* _worker;
  struct epoll_event _epoll_event;
  std::string _write_buffer;
  std::vector<char> _read_buffer;
  std::mutex _write_lock;
  std::mutex _subscription_list_lock;
  std::unique_ptr<http::Parser> _http_parser;
  websocket::Parser _websocket_parser;
  AccessController _access_controller;
  ConnectionState _state;
  bool _is_shutdown;
  bool _is_shutdown_after_flush;
  std::list<std::shared_ptr<Connection>>::iterator _connection_list_iterator;
  std::unordered_map<std::string, TopicSubscription> _subscribedTopics;

  void _enableEpollOut();
  void _disableEpollOut();
  size_t _pruneWriteBuffer(size_t bytes);
  void _parseRequest(size_t bytesRead);
};

} // namespace eventhub