import base64
import hashlib
import hmac
import http.client
import os
import random
import socket
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path


def _b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def make_jwt(secret, read=None, write=None, subject="eventhub-test"):
    if read is None:
        read = []
    if write is None:
        write = []

    header = {"alg": "HS256", "typ": "JWT"}
    payload = {
        "sub": subject,
        "read": read,
        "write": write,
    }

    header_b64 = _b64url(json_bytes(header))
    payload_b64 = _b64url(json_bytes(payload))
    signing_input = f"{header_b64}.{payload_b64}".encode("ascii")
    signature = hmac.new(secret.encode("utf-8"), signing_input, hashlib.sha256).digest()
    return f"{header_b64}.{payload_b64}.{_b64url(signature)}"


def json_bytes(obj):
    import json

    return json.dumps(obj, separators=(",", ":"), sort_keys=True).encode("utf-8")


def pick_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def wait_http_ok(host, port, path="/healthz", timeout=10.0):
    deadline = time.time() + timeout
    last_error = None

    while time.time() < deadline:
        try:
            conn = http.client.HTTPConnection(host, port, timeout=1.0)
            conn.request("GET", path)
            resp = conn.getresponse()
            if resp.status == 200:
                return
        except Exception as exc:
            last_error = exc
        time.sleep(0.2)

    raise RuntimeError(f"health check failed for {host}:{port}{path}: {last_error}")


def wait_tcp_open(host, port, timeout=5.0):
    deadline = time.time() + timeout
    last_error = None

    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return
        except Exception as exc:
            last_error = exc
        time.sleep(0.1)

    raise RuntimeError(f"tcp connect failed for {host}:{port}: {last_error}")


@dataclass
class ManagedProcess:
    process: subprocess.Popen
    name: str

    def stop(self):
        if self.process is None:
            return
        if self.process.poll() is not None:
            return
        self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except Exception:
            self.process.kill()


def start_redis(redis_bin="redis-server", port=None, quiet=True):
    if port is None:
        port = pick_free_port()

    stdout = subprocess.DEVNULL if quiet else None
    stderr = subprocess.DEVNULL if quiet else None

    args = [
        redis_bin,
        "--port",
        str(port),
        "--save",
        "",
        "--appendonly",
        "no",
        "--dir",
        "/tmp",
    ]

    proc = subprocess.Popen(args, stdout=stdout, stderr=stderr)
    return ManagedProcess(proc, "redis"), port


def start_eventhub(
    eventhub_bin,
    port,
    redis_host,
    redis_port,
    disable_auth=True,
    enable_cache=True,
    enable_sse=False,
    enable_kvstore=True,
    jwt_secret="eventhub_secret",
    quiet=True,
):
    env = os.environ.copy()
    env.update(
        {
            "LISTEN_PORT": str(port),
            "REDIS_HOST": redis_host,
            "REDIS_PORT": str(redis_port),
            "DISABLE_AUTH": "true" if disable_auth else "false",
            "ENABLE_CACHE": "true" if enable_cache else "false",
            "ENABLE_SSE": "true" if enable_sse else "false",
            "ENABLE_KVSTORE": "true" if enable_kvstore else "false",
            "JWT_SECRET": jwt_secret,
        }
    )

    stdout = subprocess.DEVNULL if quiet else None
    stderr = subprocess.DEVNULL if quiet else None

    proc = subprocess.Popen([eventhub_bin], env=env, stdout=stdout, stderr=stderr)
    return ManagedProcess(proc, "eventhub")


def add_pyclient_to_path():
    import sys

    env_path = os.environ.get("EVENTHUB_PYCLIENT_PATH")
    if env_path:
        sys.path.insert(0, env_path)
        return

    repo_root = Path(__file__).resolve().parents[2]
    default_path = repo_root.parent / "eventhub-pyclient" / "src"
    if default_path.exists():
        sys.path.insert(0, str(default_path))
        return

    raise RuntimeError(
        "eventhub-pyclient not found. Set EVENTHUB_PYCLIENT_PATH or clone it next to eventhub."
    )
