import argparse
import asyncio
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


async def test_basic(client):
    event = asyncio.Event()

    async def on_msg(topic, message):
        if topic == "e2e/basic" and message == "hello":
            event.set()

    await client.subscribe("e2e/basic", on_msg)
    await client.publish("e2e/basic", "hello")

    await asyncio.wait_for(event.wait(), timeout=5)


async def test_patterns(client):
    event_all = asyncio.Event()
    event_single = asyncio.Event()

    async def on_all(topic, message):
        if topic == "e2e/pattern/foo/bar":
            event_all.set()

    async def on_single(topic, message):
        if topic == "e2e/pattern/foo/bar":
            event_single.set()

    await client.subscribe("e2e/pattern/#", on_all)
    await client.subscribe("e2e/pattern/+/bar", on_single)
    await client.publish("e2e/pattern/foo/bar", "ping")

    await asyncio.wait_for(event_all.wait(), timeout=5)
    await asyncio.wait_for(event_single.wait(), timeout=5)


async def test_eventlog(client):
    now_ms = int(time.time() * 1000)
    await client.publish("e2e/eventlog", "m1", {"timestamp": now_ms, "ttl": 60})
    await client.publish("e2e/eventlog", "m2", {"timestamp": now_ms + 1, "ttl": 60})
    await client.publish("e2e/eventlog", "m3", {"timestamp": now_ms + 2, "ttl": 60})

    items = await client.get_eventlog("e2e/eventlog", {"since": now_ms})
    if len(items) < 3:
        raise AssertionError(f"eventlog expected >=3 items, got {len(items)}")


async def test_kvstore(client):
    await client.set("kv/test-key", "value")
    val = await client.get("kv/test-key")
    if val != "value":
        raise AssertionError(f"kv get mismatch: {val}")
    await client.delete("kv/test-key")


async def test_data_integrity(
    url,
    token,
    subscriber_count=5,
    message_count=100,
    timeout=10,
    noise_count=10,
):
    topic_suffix = int(time.time() * 1000)
    topic = f"e2e/integrity/{topic_suffix}"
    other_topic = f"{topic}/other"
    expected = [f"{topic_suffix}:{i:04d}" for i in range(message_count)]
    expected_set = set(expected)

    subscribers = []
    received = [[] for _ in range(subscriber_count)]
    wrong_topic_counts = [0 for _ in range(subscriber_count)]
    wrong_topic_examples = [[] for _ in range(subscriber_count)]
    done_events = [asyncio.Event() for _ in range(subscriber_count)]

    def make_handler(idx, expected_topic):
        async def on_msg(topic, message):
            if topic != expected_topic:
                wrong_topic_counts[idx] += 1
                if len(wrong_topic_examples[idx]) < 3:
                    wrong_topic_examples[idx].append(topic)
                return
            received[idx].append(message)
            if len(received[idx]) >= message_count:
                done_events[idx].set()

        return on_msg

    publisher = Eventhub(url, token)
    await publisher.connect()

    try:
        for i in range(subscriber_count):
            client = Eventhub(url, token)
            await client.connect()
            subscribers.append(client)
            handler = make_handler(i, topic)
            await client.subscribe(topic, handler)

        for i in range(noise_count):
            await publisher.publish(other_topic, f"{topic_suffix}:noise:{i:04d}")

        for msg in expected:
            await publisher.publish(topic, msg)

        try:
            await asyncio.wait_for(
                asyncio.gather(*[evt.wait() for evt in done_events]),
                timeout=timeout,
            )
        except asyncio.TimeoutError:
            pass

        errors = []
        for idx, items in enumerate(received):
            received_set = set(items)
            if len(items) != message_count:
                errors.append(
                    f"subscriber {idx} received {len(items)} of {message_count}"
                )
            if wrong_topic_counts[idx] > 0:
                examples = ", ".join(wrong_topic_examples[idx])
                errors.append(
                    f"subscriber {idx} received {wrong_topic_counts[idx]} messages on wrong topics ({examples})"
                )
            if received_set != expected_set:
                missing = expected_set - received_set
                extra = received_set - expected_set
                if missing:
                    errors.append(
                        f"subscriber {idx} missing {len(missing)} messages"
                    )
                if extra:
                    errors.append(
                        f"subscriber {idx} got {len(extra)} unexpected messages"
                    )
            if len(items) != len(received_set):
                errors.append(
                    f"subscriber {idx} received {len(items) - len(received_set)} duplicate messages"
                )

        if errors:
            raise AssertionError("data integrity failed: " + "; ".join(errors))
    finally:
        await publisher.disconnect()
        for client in subscribers:
            await client.disconnect()


async def test_wildcard_topic_integrity(url, token, timeout=10, messages_per_topic=5):
    topic_suffix = int(time.time() * 1000)
    base = f"e2e/wildcard/{topic_suffix}"
    cases = [
        {"name": "hash", "pattern": f"{base}/#"},
        {"name": "plus", "pattern": f"{base}/+/bar"},
    ]
    topics = [
        f"{base}",
        f"{base}/a",
        f"{base}/a/b",
        f"{base}/a/bar",
        f"{base}/x/bar",
        f"{base}/a/b/bar",
        f"{base}/a/bar/b",
        f"{base}/a/baz",
        f"{base}x",
        f"{base}x/a",
        f"{base}x/a/b",
    ]

    subscribers = []
    received = [[] for _ in cases]
    wrong_topic_counts = [0 for _ in cases]
    wrong_topic_examples = [[] for _ in cases]
    done_events = [asyncio.Event() for _ in cases]

    def filter_matches(pattern, topic):
        pattern_levels = pattern.split("/")
        topic_levels = topic.split("/")

        topic_index = 0
        for level in pattern_levels:
            if level == "#":
                return True
            if topic_index >= len(topic_levels):
                return False
            if level != "+" and level != topic_levels[topic_index]:
                return False
            topic_index += 1

        return topic_index == len(topic_levels)

    def make_handler(idx, expected_topics, expected_count):
        async def on_msg(topic, message):
            if topic not in expected_topics:
                wrong_topic_counts[idx] += 1
                if len(wrong_topic_examples[idx]) < 3:
                    wrong_topic_examples[idx].append(topic)
                return
            received[idx].append(message)
            if len(received[idx]) >= expected_count:
                done_events[idx].set()

        return on_msg

    publisher = Eventhub(url, token)
    await publisher.connect()

    try:
        for idx, case in enumerate(cases):
            client = Eventhub(url, token)
            await client.connect()
            subscribers.append(client)
            expected_topics = {t for t in topics if filter_matches(case["pattern"], t)}
            expected_count = len(expected_topics) * messages_per_topic
            handler = make_handler(idx, expected_topics, expected_count)
            await client.subscribe(case["pattern"], handler)

        for topic in topics:
            for i in range(messages_per_topic):
                payload = f"{topic}:{i:04d}"
                await publisher.publish(topic, payload)

        try:
            await asyncio.wait_for(
                asyncio.gather(*[evt.wait() for evt in done_events]),
                timeout=timeout,
            )
        except asyncio.TimeoutError:
            pass

        errors = []
        for idx, case in enumerate(cases):
            expected = set()
            for topic in topics:
                if not filter_matches(case["pattern"], topic):
                    continue
                for i in range(messages_per_topic):
                    expected.add(f"{topic}:{i:04d}")
            received_set = set(received[idx])
            if len(received[idx]) != len(expected):
                errors.append(
                    f"{case['name']} received {len(received[idx])} of {len(expected)} messages"
                )
            if received_set != expected:
                missing = expected - received_set
                extra = received_set - expected
                if missing:
                    errors.append(
                        f"{case['name']} missing {len(missing)} messages"
                    )
                if extra:
                    errors.append(
                        f"{case['name']} got {len(extra)} unexpected messages"
                    )
            if wrong_topic_counts[idx] > 0:
                examples = ", ".join(wrong_topic_examples[idx])
                errors.append(
                    f"{case['name']} received {wrong_topic_counts[idx]} messages on wrong topics ({examples})"
                )
            if len(received[idx]) != len(received_set):
                errors.append(
                    f"{case['name']} received {len(received[idx]) - len(received_set)} duplicate messages"
                )

        if errors:
            raise AssertionError("wildcard topic integrity failed: " + "; ".join(errors))
    finally:
        await publisher.disconnect()
        for client in subscribers:
            await client.disconnect()


async def run_e2e(args):
    token = ""
    if args.with_auth:
        token = make_jwt(
            args.jwt_secret,
            read=["e2e/#", "kv/#"],
            write=["e2e/#", "kv/#"],
        )

    client = Eventhub(args.url, token)
    await client.connect()

    await test_basic(client)
    await test_patterns(client)
    await test_eventlog(client)
    await test_kvstore(client)
    if args.check_data_integrity:
        await test_data_integrity(args.url, token)
        await test_wildcard_topic_integrity(args.url, token)

    await client.disconnect()


def parse_args():
    parser = argparse.ArgumentParser(description="Eventhub e2e test harness")
    parser.add_argument("--url", default="", help="WebSocket URL, e.g. ws://127.0.0.1:8080")
    parser.add_argument("--start-redis", action="store_true", help="Start a local redis-server")
    parser.add_argument("--start-eventhub", action="store_true", help="Start eventhub server process")
    parser.add_argument("--eventhub-bin", default="", help="Path to eventhub binary")
    parser.add_argument("--redis-bin", default="redis-server", help="Path to redis-server")
    parser.add_argument("--with-auth", action="store_true", help="Run tests with JWT auth enabled")
    parser.add_argument("--jwt-secret", default="eventhub_secret")
    parser.add_argument("--check-data-integrity", action="store_true", help="Verify per-subscriber message integrity")
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
                enable_cache=True,
                enable_kvstore=True,
                jwt_secret=args.jwt_secret,
                quiet=not args.verbose,
            )
            managed.append(eventhub_proc)
            wait_http_ok("127.0.0.1", port)
            args.url = f"ws://127.0.0.1:{port}"

        if not args.url:
            args.url = "ws://127.0.0.1:8080"

        asyncio.run(run_e2e(args))
        print("E2E tests OK")
    finally:
        for proc in reversed(managed):
            proc.stop()


if __name__ == "__main__":
    main()
