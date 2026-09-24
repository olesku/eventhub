# Runtime architecture

Eventhub uses one Linux epoll loop per worker. The server accepts sockets, wraps
them in an RAII handle, and moves the handle through a worker command queue. The
destination worker creates and owns the connection, its parsers, subscriptions,
timers, transport and epoll registration. Connection state is only mutated on
that worker thread.

Epoll events contain a tagged, monotonically increasing worker-local connection
ID. A worker resolves the ID in its registry for every event. Removing a
connection invalidates later kernel events, timer callbacks and subscription
snapshots without leaving a raw pointer in any of them.

`Connection` separates protocol (`HTTP`, `WEBSOCKET`, `SSE`) from lifecycle
(`OPEN`, `DRAINING`, `CLOSED`). HTTP handlers request a protocol transition; the
connection applies it only after `http::Parser::parse` and its callback return.
The parser reports the exact header bytes consumed, so a WebSocket frame that
arrives with the upgrade request is passed to the WebSocket parser unchanged.
HTTP request bodies aren't supported for WebSocket or SSE transitions.

TCP and TLS implement the same `Transport` contract. I/O results distinguish
progress, EOF, error and blocking on read or write readiness. TLS retry direction
and the pending operation are retained across epoll events. Output is stored as
owned chunks with an offset into the first chunk, keeping the memory passed to
OpenSSL stable across retries.

The per-connection output limit is 8,192,000 bytes. Low and high watermarks are
50% and 75%. Crossing them updates aggregate metrics. An outgoing WebSocket
message is framed before one atomic queue admission; if it would exceed the hard
limit, no partial message is queued and only that slow connection is closed.
Draining connections stop accepting application work and close after queued
output, with a five-second forced-close deadline.

Scheduler, topic and subscriber locks protect only their containers. Callbacks,
JSON serialization and connection writes happen after a batch or snapshot is
extracted. Unsubscribe invalidates a stable subscription ID; a later subscription
to the same filter receives a new ID. Messages already queued may still arrive,
while no new message is queued for the invalidated subscription.

Shutdown first stops command admission and wakes every worker. All workers are
asked to stop before any is joined. Each worker closes and unsubscribes its own
connections while its topic manager and shared services remain alive. Sockets
have one RAII owner and server stop is idempotent.

Handlers receive explicit configuration, Redis, KV, metrics-snapshot and
connection references. They cannot traverse the server or worker runtime. Metric
renderers consume immutable aggregate snapshots.

## Validation

Normal builds use:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --timeout 180
```

Use `-DEVENTHUB_SANITIZER=address` for ASan+UBSan or
`-DEVENTHUB_SANITIZER=thread` for TSan. The Python harness owns isolated backend
and Eventhub processes, retains their combined logs and fails on unexpected
exits or sanitizer diagnostics. It can generate a one-day localhost certificate
and verify WSS without disabling certificate validation:

```sh
python tests/harness/e2e.py --start-redis --start-eventhub \
  --eventhub-bin "$PWD/build/eventhub" --with-auth --check-data-integrity
python tests/harness/e2e.py --start-redis --start-eventhub \
  --eventhub-bin "$PWD/build/eventhub" --with-tls --with-auth \
  --check-data-integrity
```
