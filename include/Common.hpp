#ifndef INCLUDE_COMMON_HPP_
#define INCLUDE_COMMON_HPP_

#include <algorithm>
#include <cctype>
#include <string>

#include "Logger.hpp"

// How long to make epoll_wait sleep if there are no events, timers or jobs in the queue.
static constexpr unsigned int EPOLL_MAX_TIMEOUT = 100;

// How many events to maximum read in one call to epoll_wait.
static constexpr unsigned int MAXEVENTS = 1024;

// Read buffer size.
static constexpr size_t NET_READ_BUFFER_SIZE = 128;

// Max write buffer size.
static constexpr size_t NET_WRITE_BUFFER_MAX = (1024 * 1000) * 8;

// Hangup connection if data frame is larger than this.
static constexpr size_t MAX_DATA_FRAME_SIZE = (1024 * 1000) * 8;

// Will split up into continuation frames above this threshold.
static constexpr size_t WS_MAX_CHUNK_SIZE = 1 << 15;

// Hangup connection if control frame is larger than this.
static constexpr size_t WS_MAX_CONTROL_FRAME_SIZE = 1024;

// Delay metric sample rate.
static constexpr unsigned int METRIC_DELAY_SAMPLE_RATE_MS = 5000;

// Cache purger interval.
static constexpr unsigned int CACHE_PURGER_INTERVAL_MS = (60 * 1000);

// Maximum SSL handshake retries.
static const unsigned int SSL_MAX_HANDSHAKE_RETRY = 5;

#endif // INCLUDE_COMMON_HPP_
