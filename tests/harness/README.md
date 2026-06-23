# Eventhub Test Harness

This folder contains Python-based end-to-end and stress test scripts that use
the local `eventhub-pyclient` library.

## Prerequisites
- `eventhub-pyclient` cloned next to this repo, or set `EVENTHUB_PYCLIENT_PATH`
  to its `src` directory.
- Redis (optional; scripts can start their own if `--start-redis` is used).
- If Eventhub fails to start due to missing `libhiredis.so.1.1.0`, run the
  harness with `LD_LIBRARY_PATH` set to include your hiredis/redis++ libs
  (e.g. `LD_LIBRARY_PATH=./build:/usr/local/lib:/usr/lib`).

## E2E
```bash
python tests/harness/e2e.py --start-redis --start-eventhub
```

With auth:
```bash
python tests/harness/e2e.py --start-redis --start-eventhub --with-auth
```

## Stress
```bash
python tests/harness/stress.py --start-redis --start-eventhub --subscribers 100 --publishers 2 --messages 1000
```
