#ifndef INCLUDE_SSL_HPP
#define INCLUDE_SSL_HPP

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "Logger.hpp"

namespace eventhub {

template<class T> struct OpenSSLDeleter;

template<> struct OpenSSLDeleter<SSL_CTX> {
  void operator()(SSL_CTX *p) const {
    LOG->info("Free SSL_CTX");
    SSL_CTX_free(p);
  }
};

template<> struct OpenSSLDeleter<SSL> {
  void operator()(SSL *p) const {
    LOG->info("Free SSL");
    SSL_free(p);
  }
};

template<> struct OpenSSLDeleter<BIO> {
  void operator()(BIO *p) const {
    BIO_free_all(p);
  }
};

template<> struct OpenSSLDeleter<BIO_METHOD> {
  void operator()(BIO_METHOD *p) const {
    BIO_meth_free(p);
  }
};

template<class OpenSSLType>
using OpenSSLUniquePtr = std::unique_ptr<OpenSSLType, OpenSSLDeleter<OpenSSLType>>;

} // namespace eventhub

#endif