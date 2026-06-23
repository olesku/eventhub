FROM debian:stable-slim
ENV DEBIAN_FRONTEND=noninteractive

# OCI image metadata — links the image to its source repo on GHCR.
LABEL org.opencontainers.image.title="eventhub" \
      org.opencontainers.image.description="High performance pub/sub over WebSocket server written in modern C++" \
      org.opencontainers.image.source="https://github.com/olesku/eventhub" \
      org.opencontainers.image.licenses="MIT"

RUN apt-get update && \
    apt-get -qq install clang cmake git openssl libssl-dev libhiredis-dev \
    libspdlog-dev libfmt-dev ninja-build

RUN mkdir -p /usr/src/redis-plus-plus && cd /usr/src/redis-plus-plus && \
    git clone https://github.com/sewenew/redis-plus-plus.git . && \
    git checkout a63ac43bf192772910b52e27cd2b42a6098a0071 && \
    mkdir compile && cd compile && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. && \
    ninja && ninja install
# Pinned to the immutable commit for redis-plus-plus 1.3.15.

RUN mkdir -p /usr/src/eventhub
WORKDIR /usr/src/eventhub

COPY . .

RUN mkdir -p build && cd build && \
    cmake -DSKIP_TESTS=1 -GNinja -DCMAKE_BUILD_TYPE=Release .. && \
    ninja && \
    cp -a eventhub /usr/bin/eventhub && \
    cd /tmp && \
    rm -rf /usr/src/eventhub /usr/src/redis-plus-plus

WORKDIR /tmp

RUN addgroup --system eventhub && \
    adduser --system --ingroup eventhub --no-create-home --home /tmp --shell /bin/false eventhub

RUN apt-get -qq remove clang cmake git ninja-build && \
    apt-get -qq -f autoremove

USER eventhub

ENTRYPOINT [ "/usr/bin/eventhub" ]
