#ifndef INCLUDE_SSL_CONNECTION_HPP_
#define INCLUDE_SSL_CONNECTION_HPP_

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "Connection.hpp"

namespace eventhub {

class SSLConnection : public Connection {
  public:
    SSLConnection(int fd, struct sockaddr_in* csin, Server* server, Worker* worker);
    ~SSLConnection();

    ssize_t write(const string& data);
    void read();

  private:
    SSL*  _ssl;
    unsigned int _ssl_handshake_retries;

    void _init();
    void _handshake();
};

}

#endif