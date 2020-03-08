# Redis layout

**Unique ID**

Each cache item has a separate unique ID that is generated using
```HINCRBY PREFIX:topicName:cache _idCnt 1```

**Timestamp index**

Each cache-entry is indexed in a sorted set using the messages timestamp as score.
```ZSET PREFIX:topicName:scores timestamp data=MessageID```

**Storage**

Each cache entry is stored in a hash using ```HSET PREFIX:topicName:cache MessageID Data```
