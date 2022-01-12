FROM debian:bullseye-slim
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get -qq install clang cmake git openssl libssl-dev libhiredis-dev \
    libspdlog-dev libfmt-dev ninja-build

RUN mkdir -p /usr/src/redis-plus-plus && cd /usr/src/redis-plus-plus && \
    git clone https://github.com/sewenew/redis-plus-plus.git . && \
    git checkout tags/1.3.3 && \
    mkdir compile && cd compile && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. && \
    ninja && ninja install

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