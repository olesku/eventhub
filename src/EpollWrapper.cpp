#ifndef __linux__

#include "EpollWrapper.hpp"

#include <map>
#include <mutex>
#include <poll.h>
#include <stdio.h>
#include <sys/select.h>
#include <utility>

#include <iostream>

namespace {
std::size_t epfd_index = 0;
std::map<int, std::map<int, struct epoll_event*>> _events;
std::mutex mutex;
} // namespace

int epoll_create1(int flags) {
  std::lock_guard<std::mutex> lock(mutex);
  return epfd_index++;
}

int epoll_ctl(int epfd, int op, int fd,
              struct epoll_event* event) {
  std::lock_guard<std::mutex> lock(mutex);

  if (op == EPOLL_CTL_DEL) {
    _events[epfd].erase(fd);
  } else {
    _events[epfd][fd] = event;
  }

  return 0;
}

int epoll_wait_select(int epfd, struct epoll_event* events,
                      int maxevents, int timeout) {
  fd_set rfds, wfds, efds;
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  FD_ZERO(&efds);

  int maxfd = 0;

  {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it : _events[epfd]) {
      auto fd = it.first;
      auto ev = it.second;

      if (ev->events & EPOLLIN) {
        FD_SET(fd, &rfds);
      }
      if (ev->events & EPOLLOUT) {
        FD_SET(fd, &wfds);
      }
      if (ev->events & EPOLLERR) {
        FD_SET(fd, &efds);
      }

      if (fd > maxfd)
        maxfd = fd;
    }
  }

  struct timeval tv;
  tv.tv_sec  = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  int retval = select(maxfd + 1, &rfds, &wfds, &efds, (timeout < 0) ? nullptr : &tv);

  if (retval > 0) {
    int i = 0;
    for (int fd = 0; fd <= maxfd; fd++) {
      bool readyToRead  = FD_ISSET(fd, &rfds);
      bool readyToWrite = FD_ISSET(fd, &wfds);
      bool readyToErr   = FD_ISSET(fd, &efds);

      if (readyToRead || readyToWrite || readyToErr) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = _events[epfd].find(fd);
        if (it != _events[epfd].end()) {
          struct epoll_event* e = it->second;

          events[i].events = 0;

          if (readyToRead) {
            events[i].events |= EPOLLIN;
          }
          if (readyToWrite) {
            events[i].events |= EPOLLOUT;
          }
          if (readyToErr) {
            events[i].events |= EPOLLERR;
          }

          events[i].data.fd  = fd;
          events[i].data.ptr = e->data.ptr;

          i++;
        }
      }
    }
  }

  return retval;
}

int epoll_wait_poll(int epfd, struct epoll_event* events,
                    int maxevents, int timeout) {
  int nfds;
  {
    std::lock_guard<std::mutex> lock(mutex);
    nfds = _events[epfd].size();
  }

  if (nfds == 0)
    return 0;

  struct pollfd fds[nfds];

  {
    std::lock_guard<std::mutex> lock(mutex);
    int i = 0;
    for (auto it : _events[epfd]) {
      auto fd = it.first;
      auto ev = it.second;

      fds[i].fd = fd;

      fds[i].events = 0;

      if (ev->events & EPOLLIN) {
        fds[i].events |= POLLIN;
      }
      if (ev->events & EPOLLOUT) {
        fds[i].events |= POLLOUT;
      }
      if (ev->events & EPOLLERR) {
        fds[i].events |= POLLERR;
      }
      if (ev->events & EPOLLRDHUP || ev->events & EPOLLHUP) {
        fds[i].events |= POLLHUP;
      }

      i++;
    }
  }

  int retval = ::poll(fds, nfds, timeout);

  if (retval > 0) {
    int i = 0;
    for (int j = 0; j <= nfds; j++) {
      bool pollIn  = fds[j].revents & POLLIN;
      bool pollOut = fds[j].revents & POLLOUT;
      bool pollErr = fds[j].revents & POLLERR;
      bool pollHup = fds[j].revents & POLLHUP;

      if (pollIn || pollOut || pollErr || pollHup) {
        int fd = fds[j].fd;

        std::lock_guard<std::mutex> lock(mutex);
        auto it = _events[epfd].find(fd);
        if (it != _events[epfd].end()) {
          struct epoll_event* e = it->second;

          events[i].events = 0;

          if (pollIn) {
            events[i].events |= EPOLLIN;
          }
          if (pollOut) {
            events[i].events |= EPOLLOUT;
          }
          if (pollErr) {
            events[i].events |= EPOLLERR;
          }
          if (pollHup) {
            events[i].events |= EPOLLHUP;
          }

          events[i].data.fd  = fd;
          events[i].data.ptr = e->data.ptr;

          i++;
        }
      }
    }
  }

  return retval;
}

int epoll_wait(int epfd, struct epoll_event* events,
               int maxevents, int timeout) {
  return epoll_wait_poll(epfd, events, maxevents, timeout);
}

#endif
