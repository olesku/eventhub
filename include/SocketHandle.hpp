#pragma once

#include <unistd.h>

namespace eventhub {

class SocketHandle final {
public:
  SocketHandle() = default;
  explicit SocketHandle(int fd) noexcept : _fd(fd) {}
  ~SocketHandle() { reset(); }

  SocketHandle(const SocketHandle&) = delete;
  SocketHandle& operator=(const SocketHandle&) = delete;

  SocketHandle(SocketHandle&& other) noexcept : _fd(other.release()) {}
  SocketHandle& operator=(SocketHandle&& other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  int get() const noexcept { return _fd; }
  explicit operator bool() const noexcept { return _fd >= 0; }

  int release() noexcept {
    const int fd = _fd;
    _fd          = -1;
    return fd;
  }

  void reset(int fd = -1) noexcept {
    if (_fd >= 0) {
      ::close(_fd);
    }
    _fd = fd;
  }

private:
  int _fd = -1;
};

} // namespace eventhub
