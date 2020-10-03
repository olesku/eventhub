#ifndef INCLUDE_CONNECTION_HPP_
#define INCLUDE_CONNECTION_HPP_

#include <netinet/in.h>
#include <stdint.h>
#ifdef __linux__
# include <sys/epoll.h>
#else
# include "EpollWrapper.hpp"
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

#include "AccessController.hpp"
#include "Common.hpp"
#include "http/Parser.hpp"
#include "jsonrpc/jsonrpcpp.hpp"
#include "websocket/Parser.hpp"
#include "SSL.hpp"

using namespace std;

namespace eventhub {

using ConnectionPtr          = std::shared_ptr<class Connection>;
using ConnectionWeakPtr      = std::weak_ptr<class Connection>;
using ConnectionListIterator = std::list<ConnectionPtr>::iterator;

class Server;
class Worker;
class Topic;

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

class Connection : public std::enable_shared_from_this<Connection> {
public:
  Connection(int fd, struct sockaddr_in* csin, Server* server, Worker* worker);
  ~Connection();

  ssize_t write(const string& data);
  void read();
  ssize_t flushSendBuffer();

  int addToEpoll(std::list<ConnectionPtr>::iterator connectionIterator, uint32_t epollEvents);

  ConnectionState setState(ConnectionState newState);
  ConnectionState getState();
  AccessController& getAccessController();
  ConnectionListIterator getConnectionListIterator();
  ConnectionPtr getSharedPtr();
  const std::string getIP();

  void subscribe(const std::string& topicPattern, const jsonrpcpp::Id subscriptionRequestId);
  bool unsubscribe(const std::string& topicPattern);
  unsigned int unsubscribeAll();
  std::vector<std::string> listSubscriptions();

  void onHTTPRequest(http::ParserCallback callback);
  void onWebsocketRequest(websocket::ParserCallback callback);

  inline void shutdown() {
    !_is_shutdown && ::shutdown(_fd, SHUT_RDWR);
    _is_shutdown = true;
  }


  inline bool isShutdown() { return _is_shutdown; }

private:
  int _fd;
  OpenSSLUniquePtr<SSL> _ssl;
  BIO* _ssl_write_bio;
  BIO* _ssl_read_bio;
  struct sockaddr_in _csin;
  Server* _server;
  Worker* _worker;
  struct epoll_event _epoll_event;
  string _write_buffer;
  std::mutex _write_lock;
  std::mutex _subscription_list_lock;
  std::unique_ptr<http::Parser> _http_parser;
  websocket::Parser _websocket_parser;
  AccessController _access_controller;
  ConnectionState _state;
  bool _is_shutdown;
  bool _is_ssl;
  std::list<std::shared_ptr<Connection>>::iterator _connection_list_iterator;

  std::unordered_map<std::string, TopicSubscription> _subscribedTopics;

  void _enableEpollOut();
  void _disableEpollOut();
  size_t _pruneWriteBuffer(size_t bytes);
  void _initSSL();
  void _doSSLHandshake();
};

} // namespace eventhub

#endif // INCLUDE_CONNECTION_HPP_
