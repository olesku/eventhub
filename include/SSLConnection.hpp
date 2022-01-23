#pragma once

#include "Config.hpp"
#include "Forward.hpp"
#include "Connection.hpp"
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace eventhub {

class SSLConnection final : public Connection {
public:
  SSLConnection(int fd, struct sockaddr_in* csin, Worker* worker, Config& cfg, SSL_CTX* ctx);
  ~SSLConnection();

  ssize_t flushSendBuffer();
  void read();

private:
  SSL* _ssl;
  SSL_CTX* _ssl_ctx;
  unsigned int _ssl_handshake_retries;

  void _init();
  void _handshake();
};

} // namespace eventhub