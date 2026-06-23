import argparse
import asyncio
import random
import time
from pathlib import Path

from common import (
    add_pyclient_to_path,
    make_jwt,
    pick_free_port,
    start_eventhub,
    start_redis,
    wait_http_ok,
    wait_tcp_open,
)

add_pyclient_to_path()

from eventhub_client import Eventhub


class LatencyStats:
    def __init__(self, sample_limit=100000):
        self.count = 0
        self.total_ns = 0
        self.min_ns = None
        self.max_ns = None
        self.samples = []
        self.sample_limit = sample_limit

    def add(self, value_ns):
        self.count += 1
        self.total_ns += value_ns
        if self.min_ns is None or value_ns < self.min_ns:
            self.min_ns = value_ns
        if self.max_ns is None or value_ns > self.max_ns:
            self.max_ns = value_ns

        if len(self.samples) < self.sample_limit:
            self.samples.append(value_ns)
        else:
            # Reservoir sampling
            j = random.randint(0, self.count - 1)
            if j < self.sample_limit:
                self.samples[j] = value_ns

    def summary_ms(self):
        if not self.samples:
            return {}
        sorted_samples = sorted(self.samples)
        n = len(sorted_samples)

        def pct(p):
            idx = int(p * (n - 1))
            return sorted_samples[idx] / 1e6

        return {
            "count": self.count,
            "avg_ms": (self.total_ns / self.count) / 1e6 if self.count else 0,
            "min_ms": (self.min_ns or 0) / 1e6,
            "p50_ms": pct(0.50),
            "p95_ms": pct(0.95),
            "p99_ms": pct(0.99),
            "max_ms": (self.max_ns or 0) / 1e6,
        }


async def run_stress(args):
    token = ""
    if args.with_auth:
        token = make_jwt(
            args.jwt_secret,
            read=[args.topic, f"{args.topic}/#"],
            write=[args.topic, f"{args.topic}/#"],
        )

    subscribers = []
    for _ in range(args.subscribers):
        client = Eventhub(args.url, token)
        await client.connect()
        subscribers.append(client)

    stats = LatencyStats(sample_limit=args.sample_limit)
    done = asyncio.Event()
    lock = asyncio.Lock()
    expected = args.subscribers * args.publishers * args.messages
    received = 0

    async def on_msg(topic, message):
        nonlocal received
        try:
            ts_str, _ = message.split(":", 1)
            latency_ns = time.time_ns() - int(ts_str)
        except Exception:
            return

        async with lock:
            stats.add(latency_ns)
            received += 1
            if received >= expected:
                done.set()

    for client in subscribers:
        await client.subscribe(args.topic, on_msg)

    publishers = []
    payload = "x" * max(0, args.payload_bytes)

    async def publisher_task():
        client = Eventhub(args.url, token)
        await client.connect()

        delay = 0
        if args.rate > 0:
            delay = 1.0 / args.rate

        for _ in range(args.messages):
            ts = time.time_ns()
            msg = f"{ts}:{payload}"
            await client.publish(args.topic, msg)
            if delay > 0:
                await asyncio.sleep(delay)

        await client.disconnect()

    for _ in range(args.publishers):
        publishers.append(asyncio.create_task(publisher_task()))

    start = time.time()
    try:
        await asyncio.wait_for(done.wait(), timeout=args.timeout)
    except asyncio.TimeoutError:
        pass

    elapsed = time.time() - start
    for task in publishers:
        await task

    for client in subscribers:
        await client.disconnect()

    summary = stats.summary_ms()
    print("Stress summary:")
    published = args.publishers * args.messages
    print(f"  expected: {expected}")
    print(f"  received: {received}")
    print(f"  elapsed_s: {elapsed:.2f}")
    if elapsed > 0:
        print(f"  published: {published}")
        print(f"  publish_rate_msgs_s: {published / elapsed:.2f}")
        print(f"  delivery_rate_msgs_s: {received / elapsed:.2f}")
    if summary:
        print(
            f"  avg_ms: {summary['avg_ms']:.3f} p50_ms: {summary['p50_ms']:.3f} "
            f"p95_ms: {summary['p95_ms']:.3f} p99_ms: {summary['p99_ms']:.3f} "
            f"min_ms: {summary['min_ms']:.3f} max_ms: {summary['max_ms']:.3f}"
        )


def parse_args():
    parser = argparse.ArgumentParser(description="Eventhub stress test harness")
    parser.add_argument("--url", default="", help="WebSocket URL, e.g. ws://127.0.0.1:8080")
    parser.add_argument("--start-redis", action="store_true")
    parser.add_argument("--start-eventhub", action="store_true")
    parser.add_argument("--eventhub-bin", default="")
    parser.add_argument("--redis-bin", default="redis-server")
    parser.add_argument("--with-auth", action="store_true")
    parser.add_argument("--jwt-secret", default="eventhub_secret")
    parser.add_argument("--topic", default="stress/topic")
    parser.add_argument("--subscribers", type=int, default=50)
    parser.add_argument("--publishers", type=int, default=1)
    parser.add_argument("--messages", type=int, default=1000)
    parser.add_argument("--payload-bytes", type=int, default=128)
    parser.add_argument("--rate", type=float, default=0.0, help="Messages per second per publisher; 0 = no throttle")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--sample-limit", type=int, default=100000)
    parser.add_argument("--verbose", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    managed = []

    try:
        if args.start_redis:
            redis_proc, redis_port = start_redis(args.redis_bin, quiet=not args.verbose)
            managed.append(redis_proc)
            wait_tcp_open("127.0.0.1", redis_port)
        else:
            redis_port = 6379

        if args.start_eventhub:
            if not args.eventhub_bin:
                repo_root = Path(__file__).resolve().parents[2]
                args.eventhub_bin = str(repo_root / "build" / "eventhub")
            port = pick_free_port()
            eventhub_proc = start_eventhub(
                args.eventhub_bin,
                port=port,
                redis_host="127.0.0.1",
                redis_port=redis_port,
                disable_auth=not args.with_auth,
                enable_cache=False,
                enable_kvstore=True,
                jwt_secret=args.jwt_secret,
                quiet=not args.verbose,
            )
            managed.append(eventhub_proc)
            wait_http_ok("127.0.0.1", port)
            args.url = f"ws://127.0.0.1:{port}"

        if not args.url:
            args.url = "ws://127.0.0.1:8080"

        asyncio.run(run_stress(args))
    finally:
        for proc in reversed(managed):
            proc.stop()


if __name__ == "__main__":
    main()
