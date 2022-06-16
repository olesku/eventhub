# Rate limiting

Eventhub allows you to rate limit how many messages a user/token is allowed to publish within a given time period (interval). This is configured by adding ```rlimit``` configuration to the token used by the publisher.

#### Syntax
```json5
  "sub": "user@domain.com",      // Must be defined and unique for limits to work.
  "write": [ "topic1/#" ],
  "read": [ "topic1/#" ],
  "rlimit": [
    {
      "topic": "topic1/#",        // Topic or pattern to limit.
      "interval": 10,             // Bucket interval.
      "max": 10                   // Max allowed publishes within this interval.
    }
  ]
```

You can have multiple limit configuration under ```rlimit```.

#### Example
```json
  "sub": "user@domain.com",
  "write": [ "topic1/#", "topic2" ],
  "read": [ "topic1/#", "topic2" ],
  "rlimit": [
    {
      "topic": "topic1/#",
      "interval": 10,
      "max": 10
    },
    {
      "topic": "topic2",
      "interval": 10,
      "max": 10
    }
  ]
```

In cases where you have multiple limits that matches a given topic, i.e patterns and distinct topic name, the closest match will be used.
