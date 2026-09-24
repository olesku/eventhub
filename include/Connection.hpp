#pragma once

#include <netinet/in.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <ctime>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Forward.hpp"
#include "EventhubBase.hpp"
#include "http/Types.hpp"
#include "jsonrpc/jsonrpcpp.hpp"
#include "websocket/Types.hpp"

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

struct ConnectionCallbacks {
  // Callbacks run synchronously while the connection is alive. References to
  // the connection, request and payload are valid only for the callback call.
  std::function<void(Connection&, const http::Request&)> onHttpRequest;
  std::function<void(Connection&, http::ParseError)> onHttpError;
  std::function<void(Connection&, websocket::FrameType, const std::string&)> onWebSocketMessage;
  std::function<void(Connection&, websocket::ParserError)> onWebSocketError;
};

class Connection : public EventhubBase, public std::enable_shared_from_this<Connection> {
public:
  Connection(int fd, struct sockaddr_in* csin, Worker* worker, Config& cfg, ConnectionCallbacks callbacks);
  virtual ~Connection();

  void write(const std::string& data);
  virtual void read();
  virtual ssize_t flushSendBuffer();

  int addToEpoll(uint32_t epollEvents);
  int removeFromEpoll();

  ConnectionState setState(ConnectionState newState);
  ConnectionState getState();
  AccessController* getAccessController();
  void assignConnectionListIterator(std::list<ConnectionPtr>::iterator connectionIterator);
  ConnectionListIterator getConnectionListIterator();
  ConnectionPtr getSharedPtr();
  const std::string getIP();

  void subscribe(const std::string& topicPattern, const jsonrpcpp::Id subscriptionRequestId);
  bool unsubscribe(const std::string& topicPattern);
  std::size_t unsubscribeAll();
  std::vector<std::string> listSubscriptions();

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
  std::unique_ptr<AccessController> _access_controller;
  ConnectionState _state;
  bool _is_shutdown;
  bool _is_shutdown_after_flush;
  std::list<std::shared_ptr<Connection>>::iterator _connection_list_iterator;
  std::unordered_map<std::string, TopicSubscription> _subscribedTopics;

  void _enableEpollOut();
  void _disableEpollOut();
  std::size_t _pruneWriteBuffer(std::size_t bytes);
  void _parseRequest(std::size_t bytesRead);

private:
  ConnectionCallbacks _callbacks;
  std::unique_ptr<http::Parser> _http_parser;
  std::unique_ptr<websocket::Parser> _websocket_parser;
};

} // namespace eventhub
