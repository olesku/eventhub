#pragma once

#include <ctime>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <stdint.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EventhubBase.hpp"
#include "Forward.hpp"
#include "Transport.hpp"
#include "http/Types.hpp"
#include "jsonrpc/jsonrpcpp.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
using ConnectionPtr     = std::shared_ptr<Connection>;
using ConnectionWeakPtr = std::weak_ptr<Connection>;
using ConnectionId      = std::uint64_t;

enum class ConnectionProtocol {
  HTTP,
  WEBSOCKET,
  SSE
};

enum class ConnectionLifecycle {
  OPEN,
  DRAINING,
  CLOSED
};

struct TopicSubscription {
  std::shared_ptr<Topic> topic;
  std::uint64_t subscriptionId;
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
  Connection(ConnectionId id, std::unique_ptr<Transport> transport, const struct sockaddr_in& address,
             Worker* worker, Config& cfg, ConnectionCallbacks callbacks);
  virtual ~Connection();

  bool write(const std::string& data);
  virtual void read();
  virtual ssize_t flushSendBuffer();
  void handleEvents(std::uint32_t events);

  int addToEpoll(uint32_t epollEvents, std::uint64_t token);
  int removeFromEpoll();

  bool upgradeToWebSocket();
  bool startEventStream();
  ConnectionProtocol protocol() const noexcept { return _protocol; }
  ConnectionLifecycle lifecycle() const noexcept { return _lifecycle; }
  AccessController* getAccessController();
  ConnectionId id() const noexcept { return _id; }
  ConnectionPtr getSharedPtr();
  const std::string getIP();

  void subscribe(const std::string& topicPattern, const jsonrpcpp::Id subscriptionRequestId);
  bool unsubscribe(const std::string& topicPattern);
  std::size_t unsubscribeAll();
  std::vector<std::string> listSubscriptions();

  void closeAfterFlush();
  void close();
  bool isShutdown() const noexcept { return _lifecycle == ConnectionLifecycle::CLOSED; }

protected:
  int socketFd() const noexcept { return _transport->fd(); }
  ConnectionId _id;
  std::unique_ptr<Transport> _transport;
  struct sockaddr_in _csin;
  Worker* _worker;
  struct epoll_event _epoll_event;
  std::deque<std::string> _write_queue;
  std::size_t _write_offset{0};
  std::size_t _queued_bytes{0};
  bool _congested{false};
  std::vector<char> _read_buffer;
  std::mutex _write_lock;
  std::mutex _subscription_list_lock;
  std::unique_ptr<AccessController> _access_controller;
  ConnectionProtocol _protocol{ConnectionProtocol::HTTP};
  ConnectionLifecycle _lifecycle{ConnectionLifecycle::OPEN};
  std::optional<ConnectionProtocol> _pending_protocol;
  bool _is_shutdown_after_flush;
  bool _drain_timer_started{false};
  std::unordered_map<std::string, TopicSubscription> _subscribedTopics;

  void _enableEpollOut();
  void _disableEpollOut();
  bool _advanceHandshake();
  enum class PendingIo {
    NONE,
    HANDSHAKE,
    READ,
    WRITE
  };
  PendingIo _pending_io{PendingIo::NONE};
  IoWait _pending_wait{IoWait::NONE};

  void _setTransportWait(IoWait wait, PendingIo operation);
  void _updateCongestion();
  void _parseRequest(std::size_t bytesRead);
  void _applyPendingProtocol();

private:
  ConnectionCallbacks _callbacks;
  std::unique_ptr<http::Parser> _http_parser;
  std::unique_ptr<websocket::Parser> _websocket_parser;
};

} // namespace eventhub
