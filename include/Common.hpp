#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <cstddef>

#include "Logger.hpp"

// How long to make epoll_wait sleep if there are no events, timers or jobs in the queue.
static constexpr std::size_t EPOLL_MAX_TIMEOUT = 100;

// How many events to maximum read in one call to epoll_wait.
static constexpr std::size_t MAXEVENTS = 1024;

// Read buffer size.
static constexpr std::size_t NET_READ_BUFFER_SIZE = 128;

// Max write buffer size.
static constexpr std::size_t NET_WRITE_BUFFER_MAX = (1024 * 1000) * 8;

// Hangup connection if data frame is larger than this.
static constexpr std::size_t MAX_DATA_FRAME_SIZE = (1024 * 1000) * 8;

// String used in Sec-WebSocket-Accept header during websocket handshake.
static constexpr const char* WS_MAGIC_STRING = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11\0";

// Will split up into continuation frames above this threshold.
static constexpr std::size_t WS_MAX_CHUNK_SIZE = 1 << 15;

// Hangup connection if control frame is larger than this.
static constexpr std::size_t WS_MAX_CONTROL_FRAME_SIZE = 1024;

// Delay metric sample rate.
static constexpr std::size_t METRIC_DELAY_SAMPLE_RATE_MS = 5000;

// Cache purger interval.
static constexpr std::size_t CACHE_PURGER_INTERVAL_MS = (60 * 1000);

// Maximum SSL handshake retries.
static const std::size_t SSL_MAX_HANDSHAKE_RETRY = 5;


