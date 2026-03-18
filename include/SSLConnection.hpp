#pragma once

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/ossl_typ.h>
#include <sys/types.h>
#include <memory>

#include "Forward.hpp"
#include "Connection.hpp"

namespace eventhub {

class SSLConnection final : public Connection {
public:
  SSLConnection(int fd, struct sockaddr_in* csin, Worker* worker, Config& cfg, SSL_CTX* ctx);
  ~SSLConnection();

  ssize_t flushSendBuffer();
  void read();

private:
  using SSL_ptr = std::unique_ptr<SSL, decltype(&SSL_free)>;

  SSL_ptr _ssl;
  SSL_CTX* _ssl_ctx;
  unsigned int _ssl_handshake_retries;

  void _init();
  void _handshake();
};

} // namespace eventhub
