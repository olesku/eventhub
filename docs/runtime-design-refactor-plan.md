# Eventhub runtime design refactor: autonomous execution plan

Status: **implemented and locally validated; CI awaits a pull request**.

- Prepared: 2026-09-24.
- Repository: `eventhub` (original checkout: `/home/oles/code/eventhub`).
- Implementation branch: `feature/runtime-design-refactor`.
- Parent branch: `feature/modern-websocket-parser`.
- Code baseline: `fee4dafa1c8b5d094499583f42310122ec527c6e`.
- Scope: runtime ownership, scheduling, protocol boundaries, transports,
  backpressure, handler dependencies, and their verification.

This document is the complete handoff for a new session. Earlier conversation
is not required. The design below is the implementation proposal; the progress
ledger records what has actually been completed.

## Start in a new session

Use this instruction:

> Read `docs/runtime-design-refactor-plan.md` and implement the entire plan on
> `feature/runtime-design-refactor`. Work autonomously through the phases, review
> the changes, run the required tests, and commit and push the implementation.
> Keep the progress and verification ledger current so another session can
> resume. Resolve routine design details using the invariants in the plan.
> Do not merge or deploy.

When given that instruction, execute the plan rather than only revising it.
Start by reading applicable `AGENTS.md` instructions and checking the checkout:

```sh
pwd
git status --short --branch
git branch --show-current
git log -8 --oneline
git remote -v
git merge-base --is-ancestor fee4dafa1c8b5d094499583f42310122ec527c6e HEAD
```

Use the existing implementation branch if it already contains progress. Read
the ledger and recent commits before resuming; do not repeat finished phases.
Preserve unrelated user changes. Never reset, force-push, or rewrite the parent
branch. If remote commits appear, fetch and inspect them before integrating.
Branch drift or a newer baseline requires reconciling this plan with the code,
not blindly restoring the recorded commit.

Complete each phase with a focused review, relevant passing checks, and a
coherent commit. Push ordinary commits to this branch as milestones are ready.
Continue without requesting approval between phases. Fix relevant failures;
record genuinely unrelated baseline failures separately with reproduction
evidence. An unavailable dependency or sanitizer runtime is a verification
blocker, not a passing check. Continue independent work and report any remaining
blocker accurately. Ask only when a missing decision cannot reasonably be
resolved within the scope and compatibility rules below.

The final deliverable is an implemented, tested, reviewed, committed, and pushed
refactor plus updated documentation. Creating or editing a PR, merging,
publishing releases, and production deployment are outside this plan.

## Product and compatibility boundaries

Eventhub is an event gateway using Linux readiness notifications, worker threads,
Redis/Valkey PubSub, topics and wildcard subscriptions, JSON-RPC over WebSocket,
and optional SSE. Retain that architecture and C++17. Improve its internal
boundaries without replacing the networking stack or datastore client.

Preserve these contracts:

1. Keep the name `Connection`. Use `connection`, `weakConnection`,
   `connectionFd`, `request`, and `response` in touched code. Avoid new wrapper
   methods whose only purpose is forwarding parser callback registration.
2. `Connection` privately owns its protocol parsers. `ConnectionCallbacks`
   reports semantic events: `onHttpRequest`, `onHttpError`,
   `onWebSocketMessage`, and `onWebSocketError`. Protocol names disambiguate
   this mixed-protocol boundary; inside `http::Parser` or
   `websocket::Parser`, use contextual names such as `onRequest` or `onMessage`.
3. Callbacks are synchronous and receive borrowed references. `Connection&`,
   `const http::Request&`, and message payload references are valid during the
   callback only. Cross-thread work must own its data. Do not store a callback
   that keeps its own connection alive through a `shared_ptr` cycle.
4. Incomplete HTTP input is parser state, not an application callback. Keep
   `http::ParseError::INVALID_REQUEST` and `REQUEST_TOO_LARGE`; a failed HTTP
   parser reports its error once and remains terminal.
5. Preserve JSON-RPC methods, IDs, response/error shapes, authentication and
   authorization, event logging, KV behavior, wildcard matching, rate limiting,
   CORS, health and metrics routes, SSE event encoding, and existing config keys.
   Additional metrics or optional configuration must be backward compatible.
6. `http::Request::method()` currently preserves the received spelling. The
   HTTP handler lowercases a local copy and accepts `GET`, `get`, and `GeT`.
   Preserve and test this behavior; changing method strictness is separate work.
7. Keep handler names such as `handleRequest`, `handleError`, and `handleMessage`.
   Retain the recent modern C++ WebSocket parser and its framing validation.

Intended behavior corrections are: no callback-induced scheduler/topic deadlocks;
safe cross-thread handoff and shutdown; no stale-connection event access; no
lost bytes after a successful upgrade; correct partial TCP/TLS I/O and bounded
draining; and explicit slow-consumer handling. These must have regression tests.
Malformed input remains rejected. This is not an opportunity to add generic
HTTP request-body processing, HTTP pipelining, HTTP/2, HTTP/3, WebTransport,
new delivery guarantees, a plugin framework, or a broad naming/style rewrite.

## Baseline and code map

The parent branch already contains the modern parser, direct parser callback
configuration, encapsulated connection callbacks, and full `response` spelling.
Relevant commits are `0db57b2`, `0665743`, `2a24c2a`, and `fee4daf`.

Earlier work reported 27 C++ tests / 1,354 assertions, an ASan C++ run, WS
integration with and without JWT and with data integrity checks, and a temporary
WSS integration check. These are historical observations, **not evidence for
this refactor**. Re-establish the baseline. There is no recorded TSan baseline,
and the temporary WSS script is not a repository test.

| Area | Starting files | Observed reason to change |
| --- | --- | --- |
| Scheduling | `include/EventLoop.hpp`, `tests/src/EventLoopTest.cpp` | Jobs and timers execute callbacks while holding queue mutexes. |
| Worker lifecycle | `include/ConnectionWorker.hpp`, `src/ConnectionWorker.cpp`, `include/Worker.hpp`, `src/Server.cpp` | Accepted sockets can initialize on another worker's thread; stop does not wake idle epoll; connection events use raw pointers. |
| Connection | `include/Connection.hpp`, `src/Connection.cpp` | Mixed protocol, I/O, epoll and owner-container concerns; publicly unrestricted `setState`; owner list iterator stored in the connection. |
| Topic delivery | `include/Topic*.hpp`, `src/Topic.cpp`, `src/TopicManager.cpp` | Manager and subscriber locks remain held during delivery and writes. |
| HTTP input | `include/http/Parser.hpp`, `src/http/Parser.cpp` | Successful consumed length is discarded; whole read is charged to the HTTP header limit. |
| WebSocket input | `include/websocket/Parser.hpp`, `src/websocket/Parser.cpp` | Incremental framing exists; input consumption/terminal behavior needs an explicit connection-level contract. |
| TLS | `include/SSLConnection.hpp`, `src/SSLConnection.cpp` | Subclass duplicates I/O and close paths; WANT direction, read-buffer size, and draining need attention. |
| Handler dependencies | `include/HandlerContext.hpp`, `src/RPCHandler.cpp`, protocol handlers, metrics renderers | Context exposes entire server and worker instead of required capabilities. |
| Validation | `CMakeLists.txt`, `Makefile`, `tests/`, `.github/workflows/build.yaml` | Sanitizer flags can be overwritten; WSS coverage is not persistent; stress count mismatch currently only prints. |

Re-read these files before changing them. Observations are starting hypotheses
to verify against the current checkout, not permission to skip reproduction.

## Target design and invariants

### Ownership and execution

`Server` owns workers and shared services. Each worker owns a registry of its
connections, its topic/subscription state, its scheduler, and its epoll interest
registrations. `Connection` owns protocol state, parsers, output queue, and one
transport. A transport owns its socket and optional TLS session. No other
component independently closes that socket.

All connection mutation runs on the owning worker: construction/activation,
parser callbacks, state changes, authentication state, subscriptions, output,
readiness changes, timers, and destruction of worker-dependent resources.
Foreign threads submit commands with owned payloads. Published metrics are
atomic counters or explicit snapshots; reading metrics must not traverse mutable
connection state from another thread.

Use an explicit command envelope/queue for socket handoff and connection-targeted
commands. It must support move-only RAII socket ownership; do not squeeze an
unowned fd into a copied `std::function`. Existing generic scheduler jobs may
remain where their ownership is clear. Queue rejection during shutdown destroys
the envelope and releases its resources. Control/stop wakeups must remain reliable.
Do not add a bounded command queue that silently loses publications; aggregate
mailbox overload policy is separate from the per-connection output limit here.

Give each connection a worker-local monotonically increasing `ConnectionId` that
is not reused during the worker's lifetime. Use tagged `epoll_event.data.u64`
tokens for connection IDs and listener/eventfd/timerfd events. Resolve a
connection through the registry for each event; ignore IDs already removed.
Reserve tag bits, detect exhaustion, and never confuse an fd with an ID. Hold a
strong local reference while dispatching if callbacks can remove the registry
entry. Do not put raw connection pointers or owning-container iterators in epoll
events, timers, or foreign-thread commands.

### Scheduling and subscriptions

No queue, topic, or subscriber mutex is held while invoking callbacks,
serializing application messages, or doing transport I/O. Extract jobs/due timers
under their lock and execute outside it. Work enqueued by a callback runs in a
later batch. Preserve FIFO ordering within a producer's submissions, and give
timers and socket events an opportunity to progress during sustained publication.
Pending jobs must prevent an indefinite `epoll_wait` sleep.

Timer state and the next deadline have one coherent synchronization rule. Timer
context addresses remain valid through their callback. Preserve callback updates
to `repeat`/`repeat_delay` and the existing repeat delay measured from callback
completion. Newly added earlier timers cannot be lost during deadline
recalculation. Use `steady_clock`. Arm timerfd on its owning worker.

Take a snapshot of matching topics and then a snapshot of subscription handles;
release any locks before delivery. Because topics become worker-confined, remove
unneeded locks only after all call sites obey that contract. A handle contains
connection identity, subscription identity/generation, and the JSON-RPC request
ID needed for the response, not a public list iterator. Revalidate the handle
before enqueueing output. Unsubscribe invalidates it before acknowledging the
operation; resubscribing to the same filter creates a different identity.

After unsubscribe is processed, no new delivery may be enqueued for that old
subscription. Bytes already queued/sent may still arrive. Preserve per-subscription
ordering and existing duplicate behavior for overlapping subscriptions; do not
promise global order across publishers or global exactly-once delivery. Removal
of an empty topic must not erase a replacement topic created under the same name.

### Protocol and input boundaries

Separate protocol (`Http`, `WebSocket`, `Sse`) from lifecycle
(`Open`, `Draining`, `Closed`). TLS handshake progress belongs to the transport.
Expose deliberate operations such as `upgradeToWebSocket()`, `startEventStream()`,
`closeAfterFlush()`, and `close()`, with a success/failure result where useful.
These names are proposals; their restricted semantics are required. Remove the
general public `setState` operation.

Only `Http -> WebSocket` and `Http -> Sse` transitions are legal, and only while
open. A handler stages the transition after its response was accepted into the
output queue. Apply the transition after the parser callback and parse call have
returned. Then destroy the HTTP parser and feed remaining bytes to the new
protocol. A callback must never destroy the parser currently invoking it.
Draining rejects new application work while allowing required output/TLS progress;
closed is terminal. Closing and removal are idempotent.

Have parsers return an explicit result containing bytes consumed from the
**current input span** and a status such as `NeedMore`, `Complete`, `Stopped`,
or `Failed`. Use `std::string_view` or a C++17-compatible byte view. Callbacks still
carry semantic events. A parser may retain copied incomplete bytes, but must not
retain a borrowed view after `parse` returns. Consumption is between zero and the
input length, and a zero-consumption terminal result must end the driver loop.

For HTTP, stop at the end of one header block. Count only bytes through that
boundary toward the 8,192-byte header limit, including bytes buffered in previous
calls. Do not retain or count the suffix as HTTP headers. The connection handles
the suffix according to the resulting protocol/lifecycle. Ordinary HTTP handlers
currently close after their response; do not turn remaining bytes into extra
requests. Reject upgrade/SSE requests with unsupported body framing rather than
treating body bytes as WebSocket frames. Specify and test the policy for
`Content-Length` and `Transfer-Encoding` while preserving ordinary bodyless
requests. SSE remains a one-way response stream; unexpected client application
bytes close it deliberately rather than falling through an invalid-state branch.

### Transport and output

Replace `SSLConnection : Connection` with `Connection` owning a small transport
interface and `TcpTransport`/`TlsTransport` implementations. Keep socket/TLS
operations in transports, protocol callbacks and output policy in `Connection`,
and epoll registration in `Worker`. A narrow virtual interface is justified by
the two real transports and deterministic I/O tests; avoid an interface hierarchy
for every other class.

Transport results distinguish progress, would-block plus required readiness,
clean EOF, and fatal error. Cover handshake, read, write and graceful close.
TLS reads may require write readiness and TLS writes may require read readiness.
Track the pending operation and OpenSSL retry requirements; retain the same
write data and length across retries unless explicitly using and testing an
OpenSSL mode that permits otherwise. Process buffered TLS plaintext without
waiting for another socket event. Do not busy-spin on WANT or zero progress.

Use a shared output queue of owned immutable chunks plus an offset into the head
chunk. Partial writes advance the offset; avoid repeated whole-string suffix
copies. A TLS retry's backing storage must remain stable when more output arrives.
Track logical unsent bytes accurately and bound retained storage as well: a
partially consumed head chunk must not permit unbounded retained allocations.
Read destinations must have sufficient `size()`, not just `capacity()`.
Handle interrupted syscalls, would-block and EOF distinctly. Process readable
and writable flags from the same event; do not skip reads after writable work.
Drain any valid final input before acting on a peer half-close where applicable.

Use the existing connection/handshake timeout configuration where suitable.
Add one documented finite drain timeout (default 5 seconds) if no suitable close
timeout exists. It covers queued output and TLS close progress; expiry forces
cleanup. Do not require a peer's TLS close response indefinitely. Stop application
delivery as soon as draining begins.

Backpressure policy is explicit per connection:

- Keep the existing hard output limit of **8,192,000 bytes** by default. Low and
  high watermarks default to 50% and 75% of the hard limit. Validate
  `0 <= low < high < hard` if configuration is exposed.
- Crossing high records a congested state; crossing back below low clears it.
  Expose aggregate queued bytes, congested connections and slow-consumer closes
  through existing metrics formats, with no connection/topic labels.
- Reject an entire outgoing frame/event before enqueue if it would exceed the
  hard limit, then close that slow consumer. Never silently discard an accepted
  message or enqueue a truncated frame. Other subscribers must continue.
- High/low watermarks initially provide hysteresis and observability. They do
  not pause Redis globally. Do not disable all reads on a congested connection:
  TLS and protocol control traffic may require them. A more elaborate per-client
  input throttle is not required by this refactor.
- The cap bounds output retained by one connection; it is not a claim that Redis,
  the worker mailbox, or total process memory is bounded under arbitrary load.

### Handler dependencies

Replace the broad `HandlerContext` with small contexts/references that expose
what each handler uses. HTTP needs the connection, authentication/config values,
and a metrics snapshot provider; JSON-RPC needs connection/subscription operations
and the publish, event-log and KV capabilities it actually calls; SSE needs its
connection and subscription/config capabilities. Keep authorization decisions
in their present semantic layer.

Use concrete references or a small function bundle at composition boundaries;
introduce an interface only for a real behavioral seam. A new object exposing
the entire `Server` under another name does not meet this goal. Avoid replacing
one context with a constructor containing every service in the application.
Read configuration as immutable startup values unless a verified existing path
requires live updates. Metrics renderers consume snapshots rather than reaching
through `Server` into mutable workers.

## Execution phases

Execute in order. Each phase must compile and leave a reviewable commit; use
multiple commits when that makes migration and regression coverage clearer.
Ownership is established before removing topic locks or changing transport
ownership. Protocol/output contracts are established before TLS migration.

### Phase 0 — Reproducible baseline and test tooling

1. Build the baseline and record toolchain, dependency revisions, command lines,
   backend versions, test counts and results. Run ordinary C++ tests and existing
   WS e2e with/without auth and data integrity. Keep logs outside the source tree.
2. Use an isolated Redis/Valkey instance on an ephemeral local port, with
   persistence disabled. Set `REDIS_HOST`/`REDIS_PORT` for C++ tests and verify
   that they actually connect there. Some existing assertions use the
   `eventhub_test` prefix; preserve those expectations or explicitly fix test
   isolation. Never clear a developer's existing datastore.
3. Fix sanitizer configuration so CMake does not overwrite instrumentation flags.
   Prefer a project sanitizer option with target-scoped compile/link flags,
   propagated to core, executable and tests. Support separate ASan+UBSan and
   TSan builds; never combine ASan and TSan. Verify actual compile commands and
   linked runtime, not merely a successful `make asan` invocation. Keep supported
   CMake/toolchain requirements consistent with CI and Docker.
4. Remove the duplicate `AlignTrailingComments` entry in `.clang-format` if still
   present and blocking the available formatter. Format only touched project
   files, excluding vendored JSON/JWT/Catch/picohttpparser sources.
5. Make harness process management retain child logs, check unexpected exits and
   sanitizer diagnostics, and reap children after terminate/kill. A passing
   Python assertion suite with a crashing server must fail. Preserve logs on
   failure and handle expected test-initiated signal shutdown distinctly.
6. Make stress count/integrity failures produce a nonzero exit status. Wait for
   subscription acknowledgments before publishing and use bounded deadlines.
   Exact expected delivery counts require a controlled scenario and unique
   message IDs; do not interpret duplicate overlapping subscriptions as loss.
7. Capture a small repeatable performance baseline on a fixed workload (same
   clients, payloads, message count, toolchain and hardware): throughput, latency
   when measured, completion time and peak RSS. Do not optimize from one noisy run.

Gate: ordinary baseline recorded; sanitizer instrumentation confirmed; test
harness failures are observable. Known baseline bugs may have failing regression
tests introduced in the phase that fixes them; do not label them refactor failures.

Suggested commit: `test: make runtime validation reproducible`.

### Phase 1 — Run scheduler callbacks outside locks

1. Extract a job batch under the job mutex and invoke it outside the mutex.
   Leave newly queued jobs for the next batch. Ensure fairness using a bounded
   batch/time budget if needed, preserving FIFO ordering for remaining work.
2. Extract due timers into stable local storage, execute outside the timer lock,
   then merge repeating timers and recompute the next deadline under one coherent
   locking rule. Account for timers added during callback execution.
3. Route cross-thread timer additions through a worker wakeup; arm timerfd on the
   worker. Retry eventfd writes interrupted by `EINTR`; treat `EAGAIN` as an
   already pending wakeup. Surface other wakeup errors.
4. Contain callback exceptions at the dispatch boundary so a failed connection
   task closes that connection and later work still runs. A throwing timer is
   cancelled unless its task explicitly handles recovery. Log context without
   payload/secrets. Do not silently swallow allocation failure or an invariant
   violation; propagate these to controlled worker/server failure handling.
5. Add tests for a job scheduling another job/timer, a timer scheduling work and
   changing its repeat settings, concurrent producers, earlier timer insertion,
   exception cleanup, FIFO order and no lost wakeup. Use a fake clock where it
   simplifies timer tests and bounded subprocess watchdogs for deadlock cases.

Gate: scheduler regressions and ordinary suite pass; no callback executes under
a scheduler queue mutex; pending work cannot leave the worker asleep indefinitely.

Suggested commit: `refactor: dispatch event loop callbacks outside locks`.

### Phase 2 — Worker ownership, identities and shutdown

1. Introduce RAII socket ownership and `ConnectionId` registry lookup. Remove
   `ConnectionListIterator`, iterator assignment/accessors, and connection-side
   mutation of owner containers. Keep epoll registration with the worker.
   During this phase, the existing connection can own the RAII socket; phase 5
   moves that same ownership into the transport. At every intermediate commit,
   there must be exactly one closer, including on constructor failure.
2. Accept with nonblocking/close-on-exec flags. Copy the peer address into an
   owned handoff command and enqueue it to the chosen worker. Initialize
   connection/TLS state, registry entry, timers and epoll there. Handle command
   rejection, registration failure and early disconnect without leaks/double close.
3. Audit every mutating call site, including Redis callbacks, timers, publication,
   subscription deletion, metrics and destruction. Add debug owner-thread
   assertions. Route foreign-thread operations through owned commands; do not
   pass borrowed request/payload references across the queue.
4. Tag epoll tokens and use ID lookup. Make stale events, expired timers and
   commands targeting removed connections harmless, including after fd reuse.
5. Make stop idempotent and wake eventfd. Request stop on all workers before
   joining any. Quiesce new accepts and external publication producers before
   destroying their destination queues. Establish a race-safe shutdown admission
   rule; reject new commands once closing starts and release pending handoffs.
6. Close/unsubscribe all connections while their worker/topic state is alive.
   Bound server shutdown already in this phase: stop application admission,
   process or reject queued commands deliberately, and force cleanup at the
   deadline. Phase 5 improves graceful per-connection transport draining; do not
   depend on that later work to make an idle worker stop today. Join workers
   before destroying shared TLS/service state. Remove destructor paths that call
   back into already destroyed owners. Do not hold the server workers mutex while
   joining a thread that might need it.
7. Test idle server stop, repeated stop, active traffic during stop, cross-worker
   acceptance, failed handoff, fd reuse, stale timer/event, and subscription
   cleanup. Repeat bounded connect/disconnect cycles under TSan and ASan.

Gate: connection mutations are owner-thread confined; lifecycle regressions
pass; an idle or busy server stops within its documented bounded deadline.

Suggested commit: `refactor: make workers own connection lifecycle`.

### Phase 3 — Snapshot topic delivery and subscription identity

1. Replace externally stored subscriber-list iterators with opaque subscription
   IDs/generations. Keep unsubscribe and disconnect cleanup idempotent.
2. Snapshot matching topics and subscriptions before delivery. Parse immutable
   publication data outside locks and share it across subscribers where useful;
   serialize subscription-specific JSON-RPC IDs correctly.
3. Enforce worker affinity for delivery; a foreign publisher only submits work.
   Validate each snapshotted handle before queueing output. A write/close that
   removes subscriptions cannot invalidate an active traversal.
4. Remove mutexes made redundant by demonstrated worker ownership. Retain
   synchronization at actual cross-thread boundaries, including metrics.
5. Test reentrant unsubscribe/close during delivery, stale snapshot after
   unsubscribe/resubscribe, expired connections, empty-topic replacement,
   wildcard overlaps, multi-worker publication and per-subscription ordering.

Gate: no topic/subscription lock surrounds callback or I/O; existing topic tests
and pubsub integration pass; slow/closed subscribers cannot corrupt iteration.

Suggested commit: `refactor: deliver publications from subscription snapshots`.

### Phase 4 — Explicit protocol transitions and input consumption

1. Introduce separate protocol/lifecycle state and restricted transition methods.
   Replace every `setState` caller, including SSE and WebSocket handshake paths.
   Gate transitions on accepted handshake output and open lifecycle state.
   Add an enqueue acceptance result to the existing output path now; phase 5
   retains that contract when replacing its storage and transport implementation.
2. Add parser consumption results and an input driver that stages callback
   transitions, waits for parse return, retires the old parser and dispatches
   the exact remaining suffix. Stop on close/failure or lack of progress.
3. Apply the HTTP header limit at the actual header boundary, including buffered
   fragments, and define unsupported body-framing handling as described above.
   Preserve terminal errors and borrowed callback lifetimes.
4. Test every split point of representative headers/frames, upgrade plus first
   masked frame in one read, header split across reads plus multiple trailing
   frames, header exactly at/over its limit, valid small header followed by a
   frame larger than that header limit, close during callback, malformed input,
   body-framing rejection, method case preservation and illegal transitions.
5. Add a raw-socket integration regression for coalesced upgrade/frame bytes;
   ordinary WebSocket clients may send them in separate writes. Test the HTTP
   close and SSE paths so suffix handling does not invent HTTP pipelining.

Gate: parser regressions pass normally and under ASan+UBSan; bytes are neither
lost nor parsed twice; no active parser is destroyed by its own callback.

Suggested commit: `refactor: make protocol transitions consume input explicitly`.

### Phase 5 — Compose TCP/TLS transports and unify output

1. Implement the narrow transport/result contract and a deterministic fake
   transport. Implement TCP first behind the existing connection behavior.
2. Introduce the chunk/offset output queue and shared flush/close logic. Make
   enqueue success follow the acceptance contract established in phase 4 so
   handlers continue to act safely on rejected output.
3. Implement TLS handshake/read/write/close with required readiness direction,
   pending-operation retry state and stable output storage. Unify TCP/TLS
   draining, limits and lifecycle behavior; remove `SSLConnection` once all
   construction paths and tests use composition. Update source lists/forward
   declarations and eliminate dead compatibility wrappers.
4. Derive epoll interests from protocol/transport/output needs without overwriting
   another pending operation's readiness. Bound per-event I/O work for fairness.
   Handle combined readable/writable events, EOF and half-close deliberately.
5. Fake-transport tests force partial writes, would-block, read-needs-write,
   write-needs-read, interrupted calls, fatal errors, EOF, zero progress, pending
   plaintext, queue growth during retry and close timeout. Check exact bytes and
   ordering, including the handshake preceding the first protocol response.
6. Add repository-owned WSS fixtures to the Python harness. Generate a short-lived
   local test CA/server certificate with correct localhost/IP subject names,
   configure Eventhub TLS, and trust that CA in the client SSL context. Do not
   depend on `/tmp` scripts, fixed certificate paths or disabled verification.
   Cover successful and failed handshake, JWT, large/fragmented messages,
   integrity, HTTP-over-TLS response draining, and orderly/abrupt peer close.

Gate: TCP and TLS follow the same output/lifecycle policy; all WS/WSS scenarios
pass; TLS retries do not corrupt output or busy-spin; no `SSLConnection` remains.

Suggested commit: `refactor: compose connection transports and unify output`.

### Phase 6 — Backpressure policy and observability

1. Implement hard-limit admission with overflow-safe arithmetic, accurate queue
   accounting and retained-memory bounds. Define whether counters include the
   active TLS retry; they must include all unsent application bytes exactly once.
2. Add high/low hysteresis and aggregate metrics. Preserve the default hard cap;
   document any new optional configuration and its validation/defaults.
3. On cap breach, reject the whole pending frame/event and close that subscriber
   with a recorded slow-consumer reason. A control/close frame is best-effort
   within remaining capacity; never allocate beyond the cap merely to report it.
4. Test boundary sizes, partial-drain accounting, high/low transitions, TLS
   retries, oversized single messages and bounded close. Test a nonreading
   subscriber alongside a healthy subscriber for WS, WSS and SSE.

Gate: output remains bounded; healthy subscribers receive complete ordered
messages during slow-consumer isolation; metrics agree with actual lifecycle.

Suggested commit: `feat: define connection backpressure policy and metrics`.

### Phase 7 — Narrow handler dependencies

1. Inventory actual dependency use in HTTP, WebSocket, SSE and RPC handlers and
   metrics renderers. Introduce the smallest contextual references/capabilities
   that cover those calls, wired by the worker/server at composition time.
2. Remove broad server/worker access from protocol/RPC handlers. Keep connection
   references borrowed for synchronous handling; asynchronous datastore work
   must own its arguments and return through the owning worker.
3. Do not incidentally make synchronous Redis operations asynchronous or change
   their ordering/error semantics. Backend scheduling is outside this refactor.
4. Keep auth, rate limits, RPC error mapping, metrics formats and all existing
   methods behaviorally compatible. Delete the old broad context when unused.
5. Use focused handler tests where needed, then exercise auth rejection, topic
   permissions, pubsub, event log, KV, health, metrics, CORS and SSE integration.

Gate: handlers declare their dependencies without reaching through a service
locator; existing protocol behavior and client compatibility tests pass.

Suggested commit: `refactor: give handlers explicit service dependencies`.

### Phase 8 — Final review, CI and documentation

1. Review the complete baseline-to-HEAD diff for ownership, callback lifetime,
   thread confinement, lock scope, epoll identity, error paths, retry state,
   integer bounds, wire compatibility and shutdown ordering. Read the final
   design as a whole, not only individual commits. Resolve findings and retest
   the affected gates; record substantive design deviations and their evidence.
2. Run the final matrix below on the implementation being delivered. Preserve
   process logs and sanitizer output, record dependency revisions, and distinguish
   locally executed checks from CI results. Verify CI corresponds to the pushed
   commit when CI is available; do not report pending/skipped CI as passed.
3. Add durable CI coverage for WS/WSS, the new lifecycle regressions and sanitizer
   jobs with finite timeouts. Keep the existing Redis 7 / Valkey 9 compatibility
   coverage. Avoid deployment workflows or unrelated dependency upgrades.
4. Update `tests/harness/README.md`, runtime architecture documentation, and user
   docs for new backpressure/timeout settings and metrics. Include exact build
   and test commands that match the implemented options.
5. Remove transitional classes/dead methods, fix stale comments and inspect
   `git diff --check`. Confirm no certificates, local logs, dependency checkouts,
   build output or credentials are staged. Commit and push remaining work.
6. Record code commit(s) validated and the final docs-only ledger commit. Verify
   a clean worktree and equality of local HEAD and remote tracking branch.
   Report changes, test evidence, material limitations and remaining risks.

Suggested commit: `docs: document and verify runtime ownership design`.

## Build and verification recipe

The following are baseline commands. Extend them in phase 0/5 with the actual
sanitizer/TLS options and keep this section executable as the implementation
evolves. Never mark a planned flag or scenario as already supported.

Dependencies: Linux; CMake/Ninja; compatible Clang C/C++; fmt; spdlog; OpenSSL;
hiredis; redis-plus-plus; Python with the Eventhub client; isolated Redis/Valkey.
Consult `.github/workflows/build.yaml` and Dockerfiles for supported versions.
The existing CI pins redis-plus-plus to
`a63ac43bf192772910b52e27cd2b42a6098a0071`. Use installed compatible dependencies
or a reproducible local prefix; do not assume previous `/tmp/eventhub-parser-deps`
or Python environments exist. Record the exact Eventhub Python client revision
and installed dependency versions; avoid a moving client revision hiding regressions.
The client source is `https://github.com/olesku/eventhub-pyclient`; install it in
a task-local virtual environment, or set `EVENTHUB_PYCLIENT_PATH` to its `src`
directory as supported by the harness. For dependencies installed in a local
prefix, supply the corresponding CMake search paths and runtime library path
explicitly and record them with the test command.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 4
# Requires the isolated backend selected by REDIS_HOST and REDIS_PORT:
ctest --test-dir build --output-on-failure --timeout 180

python tests/harness/e2e.py --start-redis --start-eventhub \
  --eventhub-bin "$PWD/build/eventhub" --check-data-integrity
python tests/harness/e2e.py --start-redis --start-eventhub \
  --eventhub-bin "$PWD/build/eventhub" --with-auth --check-data-integrity
python tests/harness/stress.py --start-redis --start-eventhub \
  --eventhub-bin "$PWD/build/eventhub" --with-auth \
  --subscribers 100 --publishers 2 --messages 1000
```

For C++ tests, launch an ephemeral backend through a checked-in fixture/helper
and export its host/port for the child test process. Wait for readiness, preserve
its logs and reap it in a `finally`/trap. Reuse/extend `tests/harness/common.py`
rather than inventing a hardcoded shared backend. The e2e `--start-redis` option
already manages an isolated backend; use the supported executable override for
Valkey or document an equivalent isolated service setup.

Use separate out-of-tree sanitizer build directories (for example under a
task-specific `mktemp -d` directory). The current `.gitignore` does not cover all
possible sanitizer directories. Before phase 0 fixes CMake, instrumentation can
be passed via `CMAKE_C_FLAGS_RELWITHDEBINFO` and
`CMAKE_CXX_FLAGS_RELWITHDEBINFO`, together with linker flags; ordinary
`CMAKE_CXX_FLAGS` is currently overwritten. Verify instrumentation either way.
Do not broadly suppress project findings to obtain a green sanitizer run.

| Required final check | Coverage and passing condition |
| --- | --- |
| Ordinary C++ suite | Existing and added tests pass with isolated datastore; no new compiler warnings attributable to the refactor. |
| ASan + UBSan | Full C++ suite, connection lifecycle and representative WS/WSS integration; no memory/undefined-behavior reports or unexpected child exits. |
| TSan | Scheduler, command handoff, publication, metrics and stop/connect/disconnect concurrency, plus representative multi-worker integration; no project data races. Run separately from ASan. |
| WS integration | JWT enabled/disabled, auth rejection, topic/wildcard routing, integrity, event log, KV and control/close behavior. |
| WSS integration | Verified test certificate, JWT, large/fragmented messages, integrity, handshake failure, HTTP response flush and bounded shutdown. |
| Protocol boundary regressions | Coalesced upgrade/frame, every representative split point, consumed counts, header limit, terminal errors and callback destruction safety. |
| HTTP/SSE compatibility | Health, JSON/Prometheus metrics, CORS/options, method casing, SSE authentication/event encoding/disconnect and unexpected input policy. |
| Stress and slow consumers | Exact counts/IDs in lossless controlled case; expected isolated disconnect in overload case; healthy clients progress; resource counts settle after cleanup. |
| Lifecycle | Idle/busy/repeated stop, failed admission, stale events/timers, fd reuse and bounded close, with watchdog deadlines. |
| Backend matrix | Redis 7 and Valkey 9 compatibility retained; relevant tests run against both. |
| Performance comparison | Same baseline workload repeated at least three times; investigate a consistent throughput/latency regression over 10% before completion. Report noise, hardware and limits; do not invent latency data the harness does not collect. |

For scheduling/race tests use barriers, latches implemented with C++17 primitives,
condition variables and controlled clocks rather than timing-only sleeps. A
timeout that leaves a blocked `std::async` destructor cannot serve as a deadlock
test; use a subprocess watchdog where necessary. Real TLS tests complement
fake-transport tests, because rare OpenSSL WANT/retry paths are hard to force on
loopback. Do not repeat expensive full matrices after docs-only edits; record
the exact last code revision tested.

## Self-review checklist and completion criteria

Before declaring completion, answer each item with a code/test reference:

- [x] A callback can add work, unsubscribe and close without holding a container
      lock or invalidating its active traversal.
- [x] Every connection mutation/destruction path has a defined owner thread;
      every foreign-thread command owns its data and has a shutdown outcome.
- [x] Socket handoff and registry failure paths release resources exactly once.
- [x] Epoll events, timers and subscription snapshots cannot target a different
      connection/subscription after identity reuse or fd reuse.
- [x] Stop wakes idle workers, stops admission/producers, and cleans up dependent
      objects before owners; all joins and drains have a bounded outcome.
- [x] HTTP transition applies after parser return; exact suffix bytes reach the
      selected protocol and invalid/body-framed input cannot bypass validation.
- [x] TCP and TLS share output and close policy; OpenSSL retry buffers remain
      valid, readiness directions are honored, and zero progress never spins.
- [x] Backpressure admits whole messages, measures actual queue state and isolates
      a slow connection; no global memory-bound guarantee is falsely implied.
- [x] Handler dependencies are explicit; auth/wire/config contracts remain intact.
- [ ] Required validation is executed and recorded; sanitizer instrumentation and
      server exit/log checks are effective. Unresolved required gates are reported
      as incomplete, even if implementation and commits are otherwise ready.
- [x] Documentation, final review, clean worktree, ordinary push and remote SHA
      verification are complete. Parent branch is unchanged.

## Progress and verification ledger

Update this section in each phase. Keep observations separate from intentions.
Record any design adjustment with its reason, compatibility impact and verifying
test. Do not depend on earlier chat messages to explain unfinished work.

| Phase | State | Implementation commits | Evidence / next action |
| --- | --- | --- | --- |
| Planning handoff | Complete | Plan commit on this branch | Parent was clean and already pushed; new branch created from `fee4daf`; implementation not started. |
| 0 Baseline/tooling | Complete | `b70e2b5` | Target-scoped sanitizers, retained process logs, strict stress integrity and isolated backends. |
| 1 Scheduler | Complete | `921c795` | Jobs and timers are extracted before callbacks; reentrant, concurrent and throwing callback tests pass. |
| 2 Worker ownership | Complete | `69fdb65`, `5c0c89e` | RAII socket handoff, owner-thread registry, tagged IDs, eventfd stop and race-safe registry teardown. |
| 3 Topic delivery | Complete | `69fdb65` | Stable subscription IDs and lock-free delivery from revalidated snapshots. |
| 4 Protocol/input | Complete | `69fdb65` | Separate protocol/lifecycle state, staged transitions and exact parser consumption. |
| 5 Transport/output | Complete | `69fdb65` | TCP/TLS composition, shared chunk queue, retry direction and real verified WSS coverage. |
| 6 Backpressure | Complete | `69fdb65` | Atomic whole-message admission, hysteresis and aggregate metrics. |
| 7 Handler dependencies | Complete | `69fdb65` | Handlers receive explicit config, Redis, KV, metrics snapshot and connection capabilities. |
| 8 Final verification/docs | Complete with gaps below | `866d46f`, `5c0c89e`, final docs commit | Full diff review found and fixed a shutdown/metrics registry race; local matrix and documentation completed. |

For each executed check, append:

```text
Date and code revision:
Build type / compiler / sanitizer (with instrumentation evidence):
Backend and Python client revisions:
Exact command and relevant environment (no secrets):
Result, test count and elapsed time:
Log/artifact location:
Known limitations or follow-up:
```

### Final execution record

Date and code revision: 2026-09-24, code through `5c0c89e` (documentation-only
changes followed). GCC 14.2.0, CMake 3.31.6 and Ninja 1.12.1 were used. The
local backend was Redis 8.0.2 with persistence disabled on a fresh ephemeral
port for every run. The Python client was pinned to
`2f82ee36c27a01c2038e6b0da8d1736ffac9e9ce`.

The ordinary RelWithDebInfo suite passed 35 test cases and 1,372 assertions in
1.14 seconds. Separate ASan+UBSan and TSan builds passed the same suite in 1.51
and 1.39 seconds. `ldd` confirmed `libasan.so.8` plus `libubsan.so.1`, and
`libtsan.so.2`, respectively. The instrumented builds used
`-DEVENTHUB_SANITIZER=address` and `-DEVENTHUB_SANITIZER=thread`; no sanitizer
diagnostic or unexpected child exit was reported.

Repository-owned integration runs passed for plain WS without JWT, plain WS
with JWT, and verified WSS with JWT under ASan+UBSan, all with data-integrity,
wildcard, event-log, KV, HTTP/SSE compatibility, large-message and coalesced
upgrade/frame checks. An untrusted WSS certificate was rejected. A TSan run
with 20 subscribers, two publishers and 200 messages per publisher delivered
8,000/8,000 expected messages with exact IDs and no race report.

Three identical ordinary stress runs on this host each delivered 8,000/8,000.
They measured 747–799 published messages/second, 14,946–15,971 deliveries/second
and p95 latency of 4.47–4.97 ms. This records post-change repeatability; there is
no trustworthy pre-change measurement or child-process peak-RSS sample, so it
is not presented as a regression comparison.

The complete diff was reviewed for locks, ownership, parser lifetime, epoll
identity, TLS retry state, queue accounting and shutdown. That review found a
race between `Server::stop()` clearing the worker registry and concurrent metric
snapshots. `5c0c89e` now stops workers under the registry lock, joins without the
lock, and reacquires it before clearing; the ordinary suite and TSan suite plus
stress test passed after this correction. `git diff --check` also passed.

Design adjustments: `HandlerContext` remains as a named composition bundle but
no longer exposes `Server` or `Worker`; splitting it further would duplicate the
same five explicit capabilities. Watermarks and the five-second drain deadline
are constants because exposing new configuration was optional. TLS close sends
one best-effort `SSL_shutdown` before the RAII socket closes; it never waits
indefinitely for the peer.

Remaining verification gaps are recorded rather than treated as passes. CI does
not run on feature-branch pushes, so the new Redis 7 / Valkey 9, sanitizer and
WS/WSS jobs await a pull request. Only Redis 8 was available locally. Real WSS
exercised TLS behavior, but deterministic fake-transport tests do not force
every OpenSSL WANT/close permutation. The nonreading-client overload matrix for
WS, WSS and SSE, and individual failed-handoff/fd-reuse/stale-event lifecycle
regressions are not automated. These gaps leave the required-validation checkbox
open; they do not conceal a locally observed failure.
