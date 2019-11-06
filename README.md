# Eventhub: Pub/sub over WebSockets server
[![Build Status](https://travis-ci.com/olesku/eventhub.svg?branch=master)](https://travis-ci.com/olesku/eventhub)
[![Docker Repository on Quay](https://quay.io/repository/olesku/eventhub/status "Docker Repository on Quay")](https://quay.io/repository/olesku/eventhub)

Eventhub is a WebSocket message broker written in modern C++.

It's written with focus on high performance and availability, and implements the [publish-subscribe pattern](https://en.wikipedia.org/wiki/Publish%E2%80%93subscribe_pattern) and the concept of topics.

# Concepts
## Topics

A topic is a category or feed name to which records are published. Each topic can have as many subscribers and publishers as you desire. Every published record on a topic will get a distinct ID and be distributed to all clients that are subscribed to the topic or a topic pattern that matches.

A topic is segmented into paths and can contain a-z, 0-9 and /.

### Examples ###
* ```myTopic```
* ```myTopic/foo/bar```

## Topic patterns

A client can be subscribed to a number of distinct topics, topic patterns, or both.
A pattern is like a regex that matches multiple topics in a single subscription.

*Note: Patterns is used for subscriptions only. You always have to publish to a distinct topic.*

Eventhub use the same layout for patterns as MQTT where ```+``` matches a single level and ```#``` matches multiple levels.

### Examples
* ```myTopic/+/bar``` matches ```myTopic/<anything>/bar```
* ```myTopic/#``` matches ```myTopic/<anything>```


## Eventlog
Eventhub stores all published records into a log that can be requested by clients who want to get all events in or since a given time frame. For example if a client gets disconnected it can request this log to get all new events since the last event that was received.

## Authentication

When authentication is enabled Eventhub require every client to authenticate with a HS256 JWT token. The JWT token specifies which topics a client is allowed to publish and subscribe to. The token has to be hashed with the ```JWT_SECRET``` your Eventhub instance is configured with so it can be verified by the server.

Authentication token is sent to the Eventhub server either through the `Authorization` header or the ```auth``` HTTP query parameter.

**Example token**
```json
{
  "sub": "user@domain.com",
  "read": [ "topic/#", "topic2/#" ],
  "write": [ "topic1/#" ]
}
```

This token wil allow subscription to all channels under ```topic1``` and ```topic2``` and publish to any topic under ```topic1```.

Eventhub does not have a interface or API to generate these tokens for you yet. So you have to generate them in your backend or through a JWT token generator like [jwt.io](https://jwt.io/).

# Clients
* [Javascript (Browser/Node.js)](https://github.com/olesku/eventhub.js)

**Implementing your own client**

Communication with the Eventhub happens over websocket using [JSONRPC](https://www.jsonrpc.org/specification) calls.
Documentation for the protcol is not released yet, but work in process. When done you can use this documentation to implement a client yourself, but for now the easiest way is to use one of the existing clients.

# Running the server

**Eventhub depends on a Redis server with pub/sub and streams support (version 5.0 or higher).**

## Configuration options
Eventhub is configured through [environment variables](https://en.wikipedia.org/wiki/Environment_variable).

|Environment variable         |Description                          |Default value           |
|-----------------------------|-------------------------------------|------------------------|
|LISTEN_PORT                  | Port to listen on                   | 8080
|WORKER_THREADS               | Number of workers                   | 0 (number of cpu cores)
|JWT_SECRET                   | JWT Token secret                    | eventhub_secret
|REDIS_HOST                   | Redis host                          | 127.0.0.1
|REDIS_PORT                   | Redis port                          | 6379
|REDIS_PASSWORD               | Redis password                      | None
|REDIS_PREFIX                 | Prefix to use for all redis keys    | eventhub
|REDIS_POOL_SIZE              | Number of Redis connections to use  | 5
|MAX_CACHE_LENGTH             | Maximum records to store in eventlog| 1000 (0 means no limit)
|PING_INTERVAL                | Websocket ping interval             | 30
|WEBSOCKET_HANDSHAKE_TIMEOUT  | Client handshake timeout            | 15
|DISABLE_AUTH                 | Disable client authentication       | false

## Docker
The easiest way is to use our docker image.

```
# Pull image
docker pull quay.io/olesku/eventhub:latest

# Run locally with authentication disabled (for test).
# Connect to redis on my-redis-server.local.
docker run --rm -it -e DISABLE_AUTH=1 -e REDIS_HOST=my-redis-server.local -p 8080:8080 quay.io/olesku/eventhub:latest
```

## Building yourself

Required libraries:
* [Glog](https://github.com/google/glog)
* [Hiredis](https://github.com/redis/hiredis)
* [Redis-plus-plus](https://github.com/sewenew/redis-plus-plus)
* [OpenSSL](https://www.openssl.org/)

Required tooling:
* Git
* CMake
* GCC and G++

```
git clone git@github.com:olesku/eventhub.git && \
mkdir build && \
cd build && \
cmake -DSKIP_TESTS=1 .. && \
make
```

## Clustering
Eventhub has clustering capabilities, and it's easy to run multiple instances with the same datasources.
It is using Redis for intercommunication, so the only thing you have to do in is to configure each instance to use the same Redis server.

# TLS/SSL
Right now Eventhub doesn't support this natively. If you want to use this you have to front it with a loadbalancer that does the TLS-termination. It has been tested with ELB/NLB/ALB on AWS and HAProxy and NGINX on-premise.

In clustered installations you usually have a loadbalancer with these capabilities in front anyway.

However, native support for TLS is planned and will be supported eventually.

# License
Eventhub is licensed under MIT. See [LICENSE](https://github.com/olesku/eventhub/blob/LICENSE).