#include <openssl/ossl_typ.h>
#include <errno.h>
#include <openssl/err.h>
#include <openssl/ssl3.h>
#include <spdlog/logger.h>
#include <string.h>
#include <string>
#include <vector>

#include "Forward.hpp"
#include "SSLConnection.hpp"
#include "Util.hpp"
#include "Common.hpp"
#include "Logger.hpp"

namespace eventhub {

SSLConnection::SSLConnection(int fd, struct sockaddr_in* csin, Worker* worker, Config& cfg, SSL_CTX* ctx) :
  Connection(fd, csin, worker, cfg), _ssl_ctx(ctx) {
  _ssl                   = nullptr;
  _ssl_handshake_retries = 0;
  _init();
}

SSLConnection::~SSLConnection() {
  if (_ssl != nullptr) {
    SSL_free(_ssl);
  }
}

void SSLConnection::_init() {
  _ssl = SSL_new(_ssl_ctx);
  if (_ssl == nullptr) {
    _ssl = nullptr;
    LOG->error("Failed to initialize SSL object for client {}", getIP());
    shutdown();
    return;
  }

  SSL_set_fd(_ssl, _fd);
  SSL_set_accept_state(_ssl);
}

void SSLConnection::_handshake() {
  ERR_clear_error();
  int ret = SSL_accept(_ssl);

  if (_ssl_handshake_retries >= SSL_MAX_HANDSHAKE_RETRY) {
    LOG->error("Max SSL retries ({}) reached for client {}", _ssl_handshake_retries, getIP());
    shutdown();
    return;
  }

  if (ret <= 0) {
    int errorCode = SSL_get_error(_ssl, ret);
    if (errorCode == SSL_ERROR_WANT_READ ||
        errorCode == SSL_ERROR_WANT_WRITE) {
      LOG->trace("OpenSSL retry handshake. Try #{}", _ssl_handshake_retries);
    } else {
      LOG->trace("Fatal error in SSL_accept: {} for client {}", Util::getSSLErrorString(errorCode), getIP());
      shutdown();
      return;
    }
  }

  _ssl_handshake_retries++;
}

ssize_t SSLConnection::flushSendBuffer() {
  if (_write_buffer.empty() || isShutdown()) {
    _disableEpollOut();
    return 0;
  }

  std::size_t pcktSize = _write_buffer.length() > NET_READ_BUFFER_SIZE ? NET_READ_BUFFER_SIZE : _write_buffer.length();
  int ret               = SSL_write(_ssl, _write_buffer.c_str(), pcktSize);

  if (ret > 0) {
    _pruneWriteBuffer(ret);
  } else {
    int err = SSL_get_error(_ssl, ret);

    if (!(err == SSL_ERROR_SYSCALL && (errno == EAGAIN || errno == EWOULDBLOCK)) &&
        err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_WANT_READ) {
      LOG->trace("write: SSL err: {} for client {}", Util::getSSLErrorString(err), getIP());
      shutdown();
      return ret;
    }
  }

  if (_write_buffer.empty()) {
    _disableEpollOut();
  } else {
    _enableEpollOut();
  }

  return ret;
}

void SSLConnection::read() {
  _read_buffer.clear();

  if (isShutdown()) {
    return;
  }

  if (!SSL_is_init_finished(_ssl)) {
    _handshake();
    return;
  }

  std::size_t bytesRead = 0;
  int ret          = 0;

  do {
    // If more read buffer capacity is required increase it by chunks of NET_READ_BUFFER_SIZE.
    if ((bytesRead + NET_READ_BUFFER_SIZE) > _read_buffer.capacity()) {
      std::size_t newCapacity = _read_buffer.capacity() + NET_READ_BUFFER_SIZE;

      if (newCapacity > MAX_DATA_FRAME_SIZE + NET_READ_BUFFER_SIZE) {
        LOG->error("Client {} exceeded max buffer size. Disconnecting.", getIP());
        shutdown();
        return;
      }

      // Retain data we have in the _read_buffer and copy it back after call to resize().
      // We have to do this since resize() invalidates existing data.
      std::vector<char> retain;
      retain.resize(bytesRead);
      memcpy(retain.data(), _read_buffer.data(), bytesRead);

      LOG->trace("Resizing readbuffer for client {} to {}", getIP(), newCapacity);
      _read_buffer.resize(newCapacity);
      memcpy(_read_buffer.data(), retain.data(), bytesRead);
    }

    ret = SSL_read(_ssl, _read_buffer.data() + bytesRead, NET_READ_BUFFER_SIZE);

    if (ret > 0) {
      bytesRead += ret;
      continue;
    }

    int err = SSL_get_error(_ssl, ret);
    if (err == SSL_ERROR_ZERO_RETURN) {
      // Client closed the connection.
      shutdown();
      return;
    } else if (err == SSL_ERROR_SYSCALL) {
      LOG->trace("OpenSSL read error: {} for client {}", Util::getSSLErrorString(ERR_get_error()), getIP());
      shutdown();
      return;
    }
  } while (ret > 0);

  _parseRequest(bytesRead);
}

} // namespace eventhub
