#pragma once

#include <cstddef>
#include <memory>

#include <openssl/ssl.h>

#include "SocketHandle.hpp"

namespace eventhub {

enum class IoStatus {
  PROGRESS,
  WOULD_BLOCK,
  EOF_REACHED,
  ERROR
};

enum class IoWait {
  NONE,
  READ,
  WRITE
};

struct IoResult {
  IoStatus status;
  std::size_t bytes{0};
  IoWait wait{IoWait::NONE};
};

class Transport {
public:
  explicit Transport(SocketHandle socket) : _socket(std::move(socket)) {}
  virtual ~Transport() = default;

  int fd() const noexcept { return _socket.get(); }
  virtual bool ready() const noexcept = 0;
  virtual IoResult handshake() = 0;
  virtual IoResult read(char* destination, std::size_t size) = 0;
  virtual IoResult write(const char* source, std::size_t size) = 0;
  virtual bool hasPendingRead() const noexcept { return false; }
  virtual void close() noexcept;

protected:
  SocketHandle _socket;
};

std::unique_ptr<Transport> makeTcpTransport(SocketHandle socket);
std::unique_ptr<Transport> makeTlsTransport(SocketHandle socket, SSL_CTX* context);

} // namespace eventhub
