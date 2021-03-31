#include "SSLConnection.hpp"
#include "Server.hpp"
#include "Util.hpp"

namespace eventhub {

SSLConnection::SSLConnection(int fd, struct sockaddr_in* csin, Server* server, Worker* worker) :
  Connection(fd, csin, server, worker) {
    _ssl = nullptr;
    _ssl_handshake_retries = 0;
    _init();
}

SSLConnection::~SSLConnection() {
  if (_ssl != nullptr) {
    SSL_free(_ssl);
  }
}

void SSLConnection::_init() {
  _ssl = SSL_new(_server->getSSLContext());
  if (_ssl == NULL) {
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
    if (errorCode == SSL_ERROR_WANT_READ  ||
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

ssize_t SSLConnection::write(const string& data) {
  std::lock_guard<std::mutex> lock(_write_lock);
  int ret = 0;

  if (isShutdown()) {
    return 0;
  }

  if ((_write_buffer.length() + data.length()) > NET_WRITE_BUFFER_MAX) {
    _write_buffer.clear();
    shutdown();
    LOG->error("Client {} exceeded max write buffer size of {}.", getIP(), NET_WRITE_BUFFER_MAX);
    return 0;
  }

  _write_buffer.append(data);
  if (_write_buffer.empty()) {
    return 0;
  }

  unsigned int pcktSize = _write_buffer.length() > NET_READ_BUFFER_SIZE ? NET_READ_BUFFER_SIZE : _write_buffer.length();
  ret = SSL_write(_ssl, _write_buffer.c_str(), pcktSize);

  if (ret > 0) {
    _pruneWriteBuffer(ret);
  } else {
    int err = SSL_get_error(_ssl, ret);

    if (!(err == SSL_ERROR_SYSCALL && (errno == EAGAIN || errno == EWOULDBLOCK)) &&
        err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_WANT_READ)
    {
      LOG->trace("write: SSL err: {} for client {}", Util::getSSLErrorString(err), getIP());
      shutdown();
      return ret;
    }
  }

  if (_write_buffer.length() > 0) {
    _enableEpollOut();
  } else {
    _disableEpollOut();
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

  size_t bytesRead = 0;
  int ret = 0;

  do {
    // If more read buffer capacity is required increase it by chunks of NET_READ_BUFFER_SIZE.
    if (bytesRead + NET_READ_BUFFER_SIZE > _read_buffer.capacity()) {
      size_t newCapacity = _read_buffer.capacity() + NET_READ_BUFFER_SIZE;

      // TODO: Max variable should be called something else.
      if (newCapacity > WS_MAX_DATA_FRAME_SIZE + NET_READ_BUFFER_SIZE) {
        LOG->error("Client {} exceeded max buffer size. Disconnecting.", getIP());
        shutdown();
        return;
      }

      // Retain data we have in the _read_buffer and copy it back after call to resize().
      // We have to do this since resize() invalidates existing data.
      vector<char> retain;
      retain.resize(bytesRead);
      memcpy(retain.data(), _read_buffer.data(), bytesRead);

      LOG->trace("Resizing readbuffer for client {} to {}", getIP(), newCapacity);
      _read_buffer.resize(newCapacity);
      memcpy(_read_buffer.data(), retain.data(), bytesRead);
    }

    ret = SSL_read(_ssl, _read_buffer.data()+bytesRead, NET_READ_BUFFER_SIZE);

    if (ret > 0) {
      bytesRead += ret;
      continue;
    }

    int err = SSL_get_error(_ssl, ret);
    if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL) {
      LOG->error("OpenSSL read error: {} for client {}", Util::getSSLErrorString(ERR_get_error()), getIP());
      shutdown();
      return;
    }
  } while (ret > 0);

  _parseRequest(bytesRead);
}

}