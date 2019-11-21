
#include "kpoll.h"

#include <stdio.h>
#include <sys/select.h>
#include <map>
#include <mutex>

namespace {
std::map<int, struct epoll_event*> _events;
std::mutex mutex;
}

int epoll_create1(int flags) {
    return 0;
}

int epoll_ctl(int epfd, int op, int fd,
              struct epoll_event *event) {
    std::lock_guard<std::mutex> lock(mutex);

    if (op == EPOLL_CTL_DEL) {
        _events.erase(fd);
    } else {
        _events[fd] = event;
    }

    return 0;
}

int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout) {

    fd_set rfds, wfds, efds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    int maxfd = 0;

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto it : _events) {
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

            if (fd > maxfd) maxfd = fd;
        }
    }

    int retval = select(maxfd+1, &rfds, &wfds, &efds, nullptr);

    if (retval > 0) {
        int i = 0;
        for (int fd = 0; fd <= maxfd; fd++) {
            bool readyToRead = FD_ISSET(fd, &rfds);
            bool readyToWrite = FD_ISSET(fd, &wfds);
            bool readyToErr = FD_ISSET(fd, &efds);

            if (readyToRead || readyToWrite || readyToErr) {
                std::lock_guard<std::mutex> lock(mutex);
                auto it = _events.find(fd);
                if (it != _events.end()) {
                    struct epoll_event *e = it->second;

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

                    events[i].data.fd = fd;
                    events[i].data.ptr = e->data.ptr;

                    i++;
                }
            }
        }
    }

    return retval;
}
