# Server-Sent-Events (SSE) support
Eventhub supports SSE protocol for subscriptions. You will still have to use websockets/one of the official libraries for publishing messages.
In the future we might add a REST-api for publishing to bypass this requirement.

You need to configure Eventhub to enable SSE in your configuration as this is disabled per default. This is done by setting the ```enable_sse``` setting to true.

## How to subscribe using SSE
Eventhub determines wether a client wants to use SSE protocol by looking at the ```Accept``` header. If this is set to ```text/event-stream``` we initiate the client using SSE-protocol rather than Websockets. All SSE client implementations should set this header for you automatically.

The subscription topic/path is specified using the request URI. Note that you need to url-encode special characters, so # becomes %23 and + becomes %2B.
Auth token is specified either using the ```auth``` query parameter or by setting the ```Authorization``` header.

Example request using Curl:
```
# Subscribe to topic1/#.
curl -H 'Accept: text/event-stream' 'http://eventhub.local/topic1/%23?auth=<my-jwt-token>'
```

## Requesting cache / event history
| Header        | Query parameter | Description                                                         |
|---------------|-----------------|---------------------------------------------------------------------|
| Last-Event-ID | since           | Get all events since specified lastevent id when connecting         |
| N/A           | limit           | Limit returned events to this                                       |