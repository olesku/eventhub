#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "Common.hpp"
#include <arpa/inet.h>
#include <ctime>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "Topic.hpp"
#include "TopicManager.hpp"

namespace eventhub {
using namespace std;

Connection::Connection(int fd, struct sockaddr_in* csin, Worker* worker) :
  _fd(fd), _worker(worker)
{
  _epoll_fd    = -1;
  _is_shutdown = false;

  memcpy(&_csin, csin, sizeof(struct sockaddr_in));
  int flag = 1;

  // Set socket to non-blocking.
  fcntl(fd, F_SETFL, O_NONBLOCK);

  // Set KEEPALIVE on socket.
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&flag, sizeof(int));

// If we have TCP_USER_TIMEOUT set it to 10 seconds.
#ifdef TCP_USER_TIMEOUT
  int timeout = 10000;
  setsockopt(fd, SOL_TCP, TCP_USER_TIMEOUT, (char*)&timeout, sizeof(timeout));
#endif

  // Set TCP_NODELAY on socket.
  setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

  //DLOG(INFO) << "Initialized client with IP: " << getIP();

  _http_request = std::make_shared<http::RequestStateMachine>();

  // Set initial state.
  setState(ConnectionState::HTTP);
}

Connection::~Connection() {
  //DLOG(INFO) << "Destructor called for client with IP: " << getIP();

  if (_epoll_fd != -1) {
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, _fd, 0);
  }

  close(_fd);

  unsubscribeAll();
}

void Connection::_enableEpollOut() {
  if (_epoll_fd != -1 && !(_epoll_event.events & EPOLLOUT)) {
    _epoll_event.events |= EPOLLOUT;
    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _fd, &_epoll_event);
  }
}

void Connection::_disableEpollOut() {
  if (_epoll_fd != -1 && (_epoll_event.events & EPOLLOUT)) {
    _epoll_event.events &= ~EPOLLOUT;
    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _fd, &_epoll_event);
  }
}

size_t Connection::_pruneWriteBuffer(size_t bytes) {
  if (_write_buffer.length() < 1) {
    return 0;
  }

  if (bytes >= _write_buffer.length()) {
    _write_buffer.clear();
    return 0;
  }

  _write_buffer = _write_buffer.substr(bytes, string::npos);
  return _write_buffer.length();
}

ssize_t Connection::read(char* buf, size_t bytes) {
  ssize_t bytesRead = ::read(_fd, buf, bytes);

  if (bytesRead > 0) {
    buf[bytesRead] = '\0';
  } else {
    buf[0] = '\0';
  }

  return bytesRead;
}

ssize_t Connection::write(const string& data) {
  std::lock_guard<std::mutex> lock(_write_lock);
  int ret = 0;

  _write_buffer.append(data);
  if (_write_buffer.empty())
    return 0;

  ret = ::write(_fd, _write_buffer.c_str(), _write_buffer.length());

  //DLOG(INFO) << "write:" << data;

  if (ret <= 0) {
    DLOG(INFO) << getIP() << ": write error: " << strerror(errno);
    _enableEpollOut();
  } else if ((unsigned int)ret < _write_buffer.length()) {
    DLOG(INFO) << getIP() << ": Could not write() entire buffer, wrote " << ret << " of " << _write_buffer.length() << " bytes.";
    _pruneWriteBuffer(ret);
    _enableEpollOut();
  } else {
    _disableEpollOut();
    _write_buffer.clear();
  }

  return ret;
}

ssize_t Connection::flushSendBuffer() {
  return write("");
}

const string Connection::getIP() {
  char ip[32];
  inet_ntop(AF_INET, &_csin.sin_addr, (char*)&ip, 32);

  return ip;
}

int Connection::addToEpoll(int epollFd, uint32_t events) {
  _epoll_event.events  = events;
  _epoll_event.data.fd = _fd;
  int ret              = epoll_ctl(epollFd, EPOLL_CTL_ADD, _fd, &_epoll_event);

  if (ret == 0) {
    _epoll_fd = epollFd;
  }

  return ret;
}

void Connection::subscribe(const std::string &topicPattern) {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);

  auto& tm = getWorker()->getTopicManager();

  if (_subscribedTopics.count(topicPattern)) {
    return;
  }

  auto topicSubscription = tm.subscribeConnection(shared_from_this(), topicPattern);

  _subscribedTopics.emplace(std::make_pair(topicPattern, topicSubscription));
}

void Connection::unsubscribe(const std::string &topicPattern) {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);

  auto& tm = getWorker()->getTopicManager();

  if (_subscribedTopics.count(topicPattern) == 0) {
    return;
  }

  auto it = _subscribedTopics.find(topicPattern);
  auto topicObj = it->second.first;
  auto topicObjConnIterator = it->second.second;

  topicObj->deleteSubscriberByIterator(topicObjConnIterator);
  _subscribedTopics.erase(it);

  if (topicObj->getSubscriberCount() == 0) {
    tm.deleteTopic(topicPattern);
  }
}

void Connection::unsubscribeAll() {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);

  auto& tm = getWorker()->getTopicManager();

  // TODO: If erase fails here we might end up in a infinite loop.
  for (auto it = _subscribedTopics.begin(); it != _subscribedTopics.end();) {
    auto topicObj = it->second.first;
    auto topicObjConnIterator = it->second.second;
    topicObj->deleteSubscriberByIterator(topicObjConnIterator);

    if (topicObj->getSubscriberCount() == 0) {
      tm.deleteTopic(it->first);
    }

    it = _subscribedTopics.erase(it);
  }
}

std::vector<std::string> Connection::listSubscriptions() {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);

  std::vector<std::string> subscriptionList;
  for (auto& topic : _subscribedTopics) {
    subscriptionList.push_back(topic.first);
  }

  return subscriptionList;
}

} // namespace eventhub
