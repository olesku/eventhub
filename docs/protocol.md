# Protocol specification
Eventhub uses [JSON-RPC](http://www.jsonrpc.org/) over WebSocket as transport protocol.

| RPC method                          | Parameters                          | Description                                 |
|-------------------------------------|------------------------------       |---------------------------------------------|
| [subscribe](#subscribe)             | *topic, since*                      | Subscribe to a topic or pattern.
| [publish](#publish)                 | *topic, message*                    | Publish to a topic.
| [unsubscribe](#unsubscribe)         | *topic*                             | Unsubscribe from a topic or pattern.
| [unsubscribeall](#unsubscribeall)   | *None*                              | Unsubscribe from all current subscriptions.
| [list](#list)                       | *None*                              | List all current subscriptions.
| [eventlog](#eventlog)               | *topic, since, sinceEventId, limit* | Request event history for a topic.
| [get](#get)                         | *key*                               | Get key from key/value store.
| [set](#set)                         | *key, value, ttl*                   | Set key in key/value store.
| [del](#del)                         | *key*                               | Delete key in key/value store.
| [ping](#ping)                       | *None*                              | Ping the server.
| [disconnect](#disconnect)           | *None*                              | Disconnect from the server.

**Important:** Each request must have a unique `id` attribute as specified by JSON-RPC. It can be a number or a string.

If you are implementing your own client I can recommend using the nice [websocat](https://github.com/vi/websocat) client for debugging and getting familiar with the protocol. It has built in jsonrpc support using the ```--jsonrpc``` flag. This is using line-mode per default, so remember to send the request as a single line when using it.

## Example requests
## subscribe

**Request:**
```json
{
  "id": 1,
  "jsonrpc": "2.0",
  "method": "subscribe",
  "params": {
    "topic": "my/topic1",
    "since": 0
    }
}
```

*The `since` attribute can be set to a timestamp or a message id to get all events from the eventlog since that period. If unset or set to 0 eventlog will not be requested.*

**Confirmation response:**
```json
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "action": "subscribe",
    "status": "ok",
    "topic": "my/topic1"
  }
}
```

**Message received on subscribed topic/pattern response:**
```json
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "id": "1574843571767-0",
    "message": "test message topic 1",
    "topic": "my/topic1"
  }
}
```

**Important:**
All messages received on a subscribed topic or pattern will have the same `id` as your successful subscription request. You should use this to correlate a received message to a subscribed pattern or topic. This is to be able to identify pattern subscriptions.

## publish

**Request:**
```json
{
  "id": 2,
  "jsonrpc": "2.0",
  "method": "publish",
  "params": { "topic": "my/topic1", "message": "Test message" }
}
```

**Response:**
```json
{
  "id": 2,
  "jsonrpc": "2.0",
  "result": {
    "action": "publish",
    "id": "1574843571767-0",
    "status": "ok",
    "topic": "my/topic1"
  }
}
```

## unsubscribe
**Request:**
```json
{
  "id": 3,
  "jsonrpc": "2.0",
  "method": "unsubscribe",
  "params": [ "my/topic1" ]
}
```

**Response:**
```json
{
  "id": 3,
  "jsonrpc": "2.0",
  "result": {
    "unsubscribe_count": 1
  }
}
```

## unsubscribeall
**Request:**
```json
{
  "id": 4,
  "jsonrpc": "2.0",
  "method": "unsubscribeAll",
  "params": []
}
```

**Response:**
```json
{
  "id": 4,
  "jsonrpc": "2.0",
  "result": {
    "unsubscribe_count": 1
  }
}
```

## list
**Request:**
```json
{
  "id": 5,
  "jsonrpc": "2.0",
  "method": "list",
  "params": []
}
```

**Response:**
```json
{
  "id": 5,
  "jsonrpc": "2.0",
  "result": [
    "my/topic1"
  ]
}
```

## eventlog
**Request:**
```json5
{
  "id": 1,
  "jsonrpc": "2.0",
  "method": "eventlog",
  "params": {
    // All events from the past 60 seconds.
    // 'since' can also be a literal unix timestamp in milliseconds or
    // you can use 'sinceEventId' to get all events since a given
    // event id.
    "since": -60000,

    // Limit result to 100 latest events in given time period.
    "limit": 100
    }
}
```

**Response:**
```json
{
  "id": 2,
  "jsonrpc": "2.0",
  "result": {
    "action": "eventlog",
    "items": [
      {
        "id": "1661265352086-0",
        "message": "Event 1",
        "topic": "my/topic1"
      },
      {
        "id": "1661265374910-0",
        "message": "Event 1",
        "topic": "my/topic1"
      },
      {
        "id": "1661265379198-0",
        "message": "Event 2",
        "topic": "my/topic1"
      },
      {
        "id": "1661265383286-0",
        "message": "Event 3",
        "topic": "my/topic1"
      }
    ],
    "status": "ok",
    "topic": "my/topic1"
  }
}
```

## get
**Request:**
```json
{
  "id": 6,
  "jsonrpc": "2.0",
  "method": "get",
  "params": {
    "key": "my/key"
    }
}
```

**Response:**
```json
{
  "id": 6,
  "jsonrpc": "2.0",
  "result": {
    "action": "get",
    "key": "my/key",
    "value": "some value"
  }
}
```

## set
**Request:**
```json
{
  "id": 7,
  "jsonrpc": "2.0",
  "method": "set",
  "params": {
    "key": "my/key",
    "value": "some value",
    "ttl": 3600
    }
}
```

*If `ttl` attribute is omitted or set to `0` it means the key is stored without any expirity time.*

**Response:**
```json
{
  "id": 7,
  "jsonrpc": "2.0",
  "result": {
    "action": "set",
    "key": "my/key",
    "success": true
  }
}
```

## del
**Request:**
```json
{
  "id": 8,
  "jsonrpc": "2.0",
  "method": "del",
  "params": {
    "key": "my/key"
    }
}
```

**Response:**
```json
{
  "id": 8,
  "jsonrpc": "2.0",
  "result": {
    "action": "del",
    "key": "my/key",
    "success": true
  }
}
```

## ping

**Request:**
```json
{
  "id": 9,
  "jsonrpc": "2.0",
  "method": "ping",
  "params": []
}
```

**Response:**
```json
{
  "id": 9,
  "jsonrpc": "2.0",
  "result": {
    "pong": 1574846750424
  }
}
```

Contents of the `pong` attribute is the server time since epoch in milliseconds.

## disconnect
**Request:**
```json
{
  "id": 10,
  "jsonrpc": "2.0",
  "method": "disconnect",
  "params": []
}
```
