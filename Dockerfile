FROM alpine:3.10

RUN apk update && \
    apk add g++ make cmake git openssl-dev hiredis-dev git && \
    apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/community gflags-dev && \
    apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/testing glog-dev

RUN mkdir -p /usr/src/redis-plus-plus && cd /usr/src/redis-plus-plus && \
    git clone https://github.com/sewenew/redis-plus-plus.git . && \
    git checkout tags/1.1.1 && \
    mkdir compile && cd compile && cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make && make install

RUN mkdir -p /usr/src/eventhub
WORKDIR /usr/src/eventhub
COPY . .
RUN mkdir -p build && cd build && \
    sed -i 's/clang++/g++/' ../CMakeLists.txt && \
    sed -i 's/clang/gcc/' ../CMakeLists.txt && \
    cmake -DSKIP_TESTS=1 .. && \
    make -j && \
    strip eventhub && \
    cp -a eventhub /usr/bin/eventhub

WORKDIR /tmp
RUN rm -rf /usr/src/eventhub && \
    apk del g++ make cmake git

RUN addgroup -S eventhub && \
    adduser -S -G eventhub -H -h /tmp -s /bin/false eventhub

USER eventhub

ENTRYPOINT [ "/usr/bin/eventhub" ]