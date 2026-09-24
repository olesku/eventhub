#include "Transport.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/err.h>

namespace eventhub {
namespace {

IoResult socketErrorResult() {
  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return {IoStatus::WOULD_BLOCK, 0, IoWait::READ};
  }
  return {IoStatus::ERROR};
}

class TcpTransport final : public Transport {
public:
  using Transport::Transport;

  bool ready() const noexcept override { return true; }
  IoResult handshake() override { return {IoStatus::PROGRESS}; }

  IoResult read(char* destination, std::size_t size) override {
    ssize_t result;
    do {
      result = ::read(fd(), destination, size);
    } while (result < 0 && errno == EINTR);
    if (result > 0) {
      return {IoStatus::PROGRESS, static_cast<std::size_t>(result)};
    }
    if (result == 0) {
      return {IoStatus::EOF_REACHED};
    }
    return socketErrorResult();
  }

  IoResult write(const char* source, std::size_t size) override {
    ssize_t result;
    do {
      result = ::write(fd(), source, size);
    } while (result < 0 && errno == EINTR);
    if (result > 0) {
      return {IoStatus::PROGRESS, static_cast<std::size_t>(result)};
    }
    if (result == 0) {
      return {IoStatus::WOULD_BLOCK, 0, IoWait::WRITE};
    }
    auto error = socketErrorResult();
    if (error.status == IoStatus::WOULD_BLOCK) {
      error.wait = IoWait::WRITE;
    }
    return error;
  }
};

class TlsTransport final : public Transport {
public:
  TlsTransport(SocketHandle socket, SSL_CTX* context)
      : Transport(std::move(socket)), _ssl(SSL_new(context), SSL_free) {
    if (!_ssl) {
      throw std::runtime_error("SSL_new failed");
    }
    SSL_set_fd(_ssl.get(), fd());
    SSL_set_accept_state(_ssl.get());
  }

  bool ready() const noexcept override { return SSL_is_init_finished(_ssl.get()); }

  IoResult handshake() override {
    ERR_clear_error();
    const int result = SSL_accept(_ssl.get());
    return translate(result, false);
  }

  IoResult read(char* destination, std::size_t size) override {
    ERR_clear_error();
    const int count = static_cast<int>(std::min<std::size_t>(size, INT_MAX));
    return translate(SSL_read(_ssl.get(), destination, count), false);
  }

  IoResult write(const char* source, std::size_t size) override {
    ERR_clear_error();
    const int count = static_cast<int>(std::min<std::size_t>(size, INT_MAX));
    return translate(SSL_write(_ssl.get(), source, count), true);
  }

  bool hasPendingRead() const noexcept override { return SSL_pending(_ssl.get()) > 0; }

  void close() noexcept override {
    if (_ssl && SSL_is_init_finished(_ssl.get())) {
      SSL_shutdown(_ssl.get());
    }
    Transport::close();
  }

private:
  using SslPtr = std::unique_ptr<SSL, decltype(&SSL_free)>;
  SslPtr _ssl;

  IoResult translate(int result, bool writing) {
    if (result > 0) {
      return {IoStatus::PROGRESS, static_cast<std::size_t>(result)};
    }
    const int error = SSL_get_error(_ssl.get(), result);
    switch (error) {
      case SSL_ERROR_WANT_READ:
        return {IoStatus::WOULD_BLOCK, 0, IoWait::READ};
      case SSL_ERROR_WANT_WRITE:
        return {IoStatus::WOULD_BLOCK, 0, IoWait::WRITE};
      case SSL_ERROR_ZERO_RETURN:
        return {IoStatus::EOF_REACHED};
      case SSL_ERROR_SYSCALL:
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return {IoStatus::WOULD_BLOCK, 0, writing ? IoWait::WRITE : IoWait::READ};
        }
        return result == 0 ? IoResult{IoStatus::EOF_REACHED} : IoResult{IoStatus::ERROR};
      default:
        return {IoStatus::ERROR};
    }
  }
};

} // namespace

void Transport::close() noexcept {
  if (_socket) {
    ::shutdown(fd(), SHUT_RDWR);
  }
}

std::unique_ptr<Transport> makeTcpTransport(SocketHandle socket) {
  return std::make_unique<TcpTransport>(std::move(socket));
}

std::unique_ptr<Transport> makeTlsTransport(SocketHandle socket, SSL_CTX* context) {
  return std::make_unique<TlsTransport>(std::move(socket), context);
}

} // namespace eventhub
