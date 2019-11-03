#ifndef EVENTHUB_COMMON_HPP
#define EVENTHUB_COMMON_HPP

#undef NDEBUG
#include <algorithm>
#include <cctype>
#include <glog/logging.h>
#include <string>

// How long to make epoll_wait sleep if there are no events, timers or jobs in the queue.
static constexpr unsigned int EPOLL_MAX_TIMEOUT = 100;

// How many events to maximum read in one call to epoll_wait.
static constexpr unsigned int MAXEVENTS = 1024;

// Read buffer size.
static constexpr size_t NET_READ_BUFFER_SIZE = 1 << 7;

// Will split up into continuation frames above this threshold.
static constexpr size_t WS_MAX_CHUNK_SIZE = 1 << 15;

// Hangup connection if data frame is larger than this.
static constexpr size_t WS_MAX_DATA_FRAME_SIZE = (1024*1000)*8;

// Hangup connection if control frame is larger than this.
static constexpr size_t WS_MAX_CONTROL_FRAME_SIZE = 1024;

#endif
