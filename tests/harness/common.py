import base64
import hashlib
import hmac
import http.client
import os
import random
import socket
import subprocess
import tempfile
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
    log_path: Path

    def stop(self):
        if self.process is None:
            return
        exited_early = self.process.poll() is not None
        if not exited_early:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)

        log = self.log_path.read_text(errors="replace") if self.log_path.exists() else ""
        sanitizer_markers = (
            "AddressSanitizer",
            "ThreadSanitizer",
            "UndefinedBehaviorSanitizer",
            "runtime error:",
        )
        sanitizer_failure = next((marker for marker in sanitizer_markers if marker in log), None)
        if sanitizer_failure:
            raise RuntimeError(
                f"{self.name} reported {sanitizer_failure}; log retained at {self.log_path}"
            )
        if exited_early and self.process.returncode != 0:
            raise RuntimeError(
                f"{self.name} exited unexpectedly with {self.process.returncode}; "
                f"log retained at {self.log_path}"
            )


def _start_logged_process(args, name, env=None, quiet=True):
    log_file = tempfile.NamedTemporaryFile(
        mode="w+b", prefix=f"eventhub-{name}-", suffix=".log", delete=False
    )
    log_path = Path(log_file.name)
    process = subprocess.Popen(args, env=env, stdout=log_file, stderr=subprocess.STDOUT)
    log_file.close()
    if not quiet:
        print(f"{name} log: {log_path}")
    return ManagedProcess(process, name, log_path)


def start_redis(redis_bin="redis-server", port=None, quiet=True):
    if port is None:
        port = pick_free_port()

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

    return _start_logged_process(args, "redis", quiet=quiet), port


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

    return _start_logged_process([eventhub_bin], "eventhub", env=env, quiet=quiet)


def add_pyclient_to_path():
    import sys

    try:
        import eventhub_client  # noqa: F401  # already installed as a package
        return
    except ImportError:
        pass

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
