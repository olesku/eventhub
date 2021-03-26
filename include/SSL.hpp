#ifndef INCLUDE_SSL_HPP
#define INCLUDE_SSL_HPP

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/bioerr.h>
#include "Logger.hpp"

namespace eventhub {

template<class T> struct OpenSSLDeleter;

template<> struct OpenSSLDeleter<SSL_CTX> {
  void operator()(SSL_CTX *p) const {
    SSL_CTX_free(p);
  }
};

template<> struct OpenSSLDeleter<SSL> {
  void operator()(SSL *p) const {
    SSL_free(p);
  }
};

template<class OpenSSLType>
using OpenSSLUniquePtr = std::unique_ptr<OpenSSLType, OpenSSLDeleter<OpenSSLType>>;

} // namespace eventhub

#endif
