#!/usr/bin/env python3
"""Concurrent SET/GET workload against the custom Redis server.

Usage:
    python3 test_multithread.py --host 127.0.0.1 --port 40000

The script opens multiple TCP connections and issues batches of SET/GET
commands simultaneously to verify that the server behaves correctly under
multi-threaded load.
"""

import argparse
import random
import socket
import struct
import threading
from typing import List


TAG_NIL = 0
TAG_ERR = 1
TAG_STR = 2
TAG_INT = 3
TAG_DBL = 4


def encode(cmd: List[str]) -> bytes:
    payload = struct.pack("<I", len(cmd))
    for token in cmd:
        data = token.encode("utf-8")
        payload += struct.pack("<I", len(data)) + data
    return struct.pack("<I", len(payload)) + payload


def recv_exact(sock: socket.socket, size: int) -> bytes:
    buf = bytearray()
    while len(buf) < size:
        chunk = sock.recv(size - len(buf))
        if not chunk:
            raise RuntimeError("connection closed unexpectedly")
        buf.extend(chunk)
    return bytes(buf)


def recv_frame(sock: socket.socket) -> bytes:
    header = recv_exact(sock, 4)
    (length,) = struct.unpack("<I", header)
    if length == 0:
        return b""
    return recv_exact(sock, length)


def parse_payload(payload: bytes):
    if not payload:
        return ("empty", None)

    if len(payload) >= 5:
        (bulk_len,) = struct.unpack_from("<I", payload, 0)
        tag = payload[4]
        if tag == TAG_STR and len(payload) >= 9:
            (str_len,) = struct.unpack_from("<I", payload, 5)
            if str_len == bulk_len and len(payload) >= 9 + str_len:
                value = payload[9:9 + str_len].decode("utf-8", errors="replace")
                return ("str", value)

    tag = payload[0]
    if tag == TAG_NIL:
        return ("nil", None)
    if tag == TAG_ERR and len(payload) >= 9:
        code = struct.unpack_from("<I", payload, 1)[0]
        msg_len = struct.unpack_from("<I", payload, 5)[0]
        msg = payload[9:9 + msg_len].decode("utf-8", errors="replace")
        return ("err", (code, msg))
    if tag == TAG_INT and len(payload) >= 9:
        val = struct.unpack_from("<q", payload, 1)[0]
        return ("int", val)
    if tag == TAG_DBL and len(payload) >= 9:
        val = struct.unpack_from("<d", payload, 1)[0]
        return ("double", val)
    if tag == 5:
        return ("array", payload)
    return ("unknown", payload)


def run_worker(tid: int, host: str, port: int, ops: int, report: list, lock: threading.Lock):
    try:
        sock = socket.create_connection((host, port), timeout=5)
    except Exception as exc:  # pragma: no cover - network failure path
        with lock:
            report.append((tid, False, f"connect failed: {exc}"))
        return

    try:
        for i in range(ops):
            key = f"k{tid}_{i}"
            value = f"v{tid}_{random.randint(0, 1_000_000)}"

            sock.sendall(encode(["set", key, value]))
            resp = parse_payload(recv_frame(sock))
            if resp[0] != "nil":
                raise RuntimeError(f"SET unexpected response: {resp}")

            sock.sendall(encode(["get", key]))
            resp = parse_payload(recv_frame(sock))
            if resp[0] != "str" or resp[1] != value:
                raise RuntimeError(f"GET mismatch: expected {value}, got {resp}")

        with lock:
            report.append((tid, True, f"{ops} ops ok"))
    except Exception as exc:
        with lock:
            report.append((tid, False, str(exc)))
    finally:
        sock.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Concurrent workload tester")
    parser.add_argument("--host", default="127.0.0.1", help="server host")
    parser.add_argument("--port", type=int, default=40000, help="server port")
    parser.add_argument("--threads", type=int, default=8, help="number of client threads")
    parser.add_argument("--ops", type=int, default=50, help="operations per thread")
    args = parser.parse_args()

    results = []
    lock = threading.Lock()
    threads = [
        threading.Thread(
            target=run_worker,
            args=(tid, args.host, args.port, args.ops, results, lock),
            daemon=True,
        )
        for tid in range(args.threads)
    ]

    for th in threads:
        th.start()
    for th in threads:
        th.join()

    success = sum(1 for _, ok, _ in results if ok)
    failures = [(tid, msg) for tid, ok, msg in results if not ok]

    print(f"Threads succeeded: {success}/{len(results)}")
    if failures:
        for tid, msg in failures:
            print(f"Thread {tid} failed: {msg}")
    else:
        print("All thread operations validated successfully.")


if __name__ == "__main__":
    main()

