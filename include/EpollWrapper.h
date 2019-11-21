#pragma once

#include <stdint.h>

enum EPOLL_EVENTS
  {
    EPOLLIN = 0x001,
#define EPOLLIN EPOLLIN
    EPOLLPRI = 0x002,
#define EPOLLPRI EPOLLPRI
    EPOLLOUT = 0x004,
#define EPOLLOUT EPOLLOUT
    EPOLLRDNORM = 0x040,
#define EPOLLRDNORM EPOLLRDNORM
    EPOLLRDBAND = 0x080,
#define EPOLLRDBAND EPOLLRDBAND
    EPOLLWRNORM = 0x100,
#define EPOLLWRNORM EPOLLWRNORM
    EPOLLWRBAND = 0x200,
#define EPOLLWRBAND EPOLLWRBAND
    EPOLLMSG = 0x400,
#define EPOLLMSG EPOLLMSG
    EPOLLERR = 0x008,
#define EPOLLERR EPOLLERR
    EPOLLHUP = 0x010,
#define EPOLLHUP EPOLLHUP
    EPOLLRDHUP = 0x2000,
#define EPOLLRDHUP EPOLLRDHUP
    EPOLLEXCLUSIVE = 1u << 28,
#define EPOLLEXCLUSIVE EPOLLEXCLUSIVE
    EPOLLWAKEUP = 1u << 29,
#define EPOLLWAKEUP EPOLLWAKEUP
    EPOLLONESHOT = 1u << 30,
#define EPOLLONESHOT EPOLLONESHOT
    EPOLLET = 1u << 31
#define EPOLLET EPOLLET
  };

/* Valid opcodes ( "op" parameter ) to issue to epoll_ctl().  */
#define EPOLL_CTL_ADD 1        /* Add a file descriptor to the interface.  */
#define EPOLL_CTL_DEL 2        /* Remove a file descriptor from the interface.  */
#define EPOLL_CTL_MOD 3        /* Change file descriptor epoll_event structure.  */
typedef union epoll_data
{
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} epoll_data_t;

struct epoll_event
{
  uint32_t events;        /* Epoll events */
  epoll_data_t data;      /* User data variable */
};

// Same as epoll_create but with an FLAGS parameter.  The unused SIZE
// parameter has been dropped.
int epoll_create1(int flags);

// Manipulate an epoll instance "epfd". Returns 0 in case of success,
// -1 in case of error ( the "errno" variable will contain the
// specific error code ) The "op" parameter is one of the EPOLL_CTL_*
// constants defined above. The "fd" parameter is the target of the
// operation. The "event" parameter describes which events the caller
// is interested in and any associated user data.
int epoll_ctl(int epfd, int op, int fd,
              struct epoll_event *event);

// Wait for events on an epoll instance "epfd". Returns the number of
// triggered events returned in "events" buffer. Or -1 in case of
// error with the "errno" variable set to the specific error code. The
// "events" parameter is a buffer that will contain triggered
// events. The "maxevents" is the maximum number of events to be
// returned ( usually size of "events" ). The "timeout" parameter
// specifies the maximum wait time in milliseconds (-1 == infinite).
int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);
