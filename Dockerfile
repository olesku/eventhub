FROM alpine:3.11

RUN apk update && \
    apk add gcc g++ make cmake ninja git openssl-dev hiredis-dev git && \
    apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/community spdlog && \
    apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/community spdlog-dev && \
    apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/community fmt-dev && \
    apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/community fmt

RUN mkdir -p /usr/src/redis-plus-plus && cd /usr/src/redis-plus-plus && \
    git clone https://github.com/sewenew/redis-plus-plus.git . && \
    git checkout tags/1.1.1 && \
    mkdir compile && cd compile && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. && \
    ninja -j0 && ninja install

RUN mkdir -p /usr/src/eventhub
WORKDIR /usr/src/eventhub
COPY . .
RUN mkdir -p build && cd build && \
    sed -i 's/clang++/g++/' ../CMakeLists.txt && \
    sed -i 's/clang/gcc/' ../CMakeLists.txt && \
    cmake -GNinja -DSKIP_TESTS=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
    ninja -j0 && \
    strip eventhub && \
    cp -a eventhub /usr/bin/eventhub

WORKDIR /tmp
RUN rm -rf /usr/src/eventhub /usr/src/redis-plus-plus && \
    apk del gcc g++ make cmake git

RUN addgroup -S eventhub && \
    adduser -S -G eventhub -H -h /tmp -s /bin/false eventhub

USER eventhub

ENTRYPOINT [ "/usr/bin/eventhub" ]