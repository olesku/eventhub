#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <sstream>
#include <stdint.h>
#include <string>

#include "common.hpp"
#include "util.hpp"

namespace eventhub {
const std::string Util::base64Encode(const unsigned char* buffer, size_t length) {
  BIO *bio, *b64;
  BUF_MEM* bufferPtr;
  std::string s;

  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);

  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(bio, buffer, length);
  BIO_flush(bio);
  BIO_get_mem_ptr(bio, &bufferPtr);
  s.assign(bufferPtr->data, bufferPtr->length);
  BIO_set_close(bio, BIO_CLOSE);
  BIO_free_all(bio);

  return s;
}
} // namespace eventhub
