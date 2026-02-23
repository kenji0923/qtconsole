#!/usr/bin/env python3
"""Sample realtime sender for qtconsole.

Supports UDP and WebSocket text payloads containing one floating-point value.
"""

from __future__ import annotations

import argparse
import asyncio
import math
import random
import socket
import sys
import time
from typing import Optional


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Send sample values to qtconsole")
    parser.add_argument("--mode", choices=["udp", "ws"], default="udp", help="Transport mode")
    parser.add_argument("--host", default="127.0.0.1", help="Destination host")
    parser.add_argument("--port", type=int, default=9000, help="Destination port")
    parser.add_argument("--hz", type=float, default=60.0, help="Send rate in Hz")
    parser.add_argument("--count", type=int, default=0, help="Number of samples (0 = infinite)")
    parser.add_argument(
        "--pattern",
        choices=["sine", "noise", "ramp"],
        default="sine",
        help="Signal pattern",
    )
    parser.add_argument("--amplitude", type=float, default=100.0, help="Signal amplitude")
    parser.add_argument("--offset", type=float, default=100.0, help="Signal offset")
    parser.add_argument("--sine-period", type=float, default=2.0, help="Sine period in seconds")
    parser.add_argument("--ramp-period", type=float, default=3.0, help="Ramp period in seconds")
    return parser.parse_args()


def make_value(pattern: str, t: float, amplitude: float, offset: float, sine_period: float, ramp_period: float) -> float:
    if pattern == "noise":
        return offset + random.uniform(-amplitude, amplitude)

    if pattern == "ramp":
        phase = (t % ramp_period) / ramp_period
        return offset + (2.0 * phase - 1.0) * amplitude

    # default sine
    angle = 2.0 * math.pi * t / sine_period
    return offset + amplitude * math.sin(angle)


def send_udp(args: argparse.Namespace) -> None:
    interval = 1.0 / args.hz
    sent = 0
    start = time.perf_counter()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        while args.count == 0 or sent < args.count:
            now = time.perf_counter() - start
            value = make_value(
                args.pattern,
                now,
                args.amplitude,
                args.offset,
                args.sine_period,
                args.ramp_period,
            )
            payload = f"{value:.6f}".encode("utf-8")
            sock.sendto(payload, (args.host, args.port))
            sent += 1

            next_deadline = start + sent * interval
            sleep_for = next_deadline - time.perf_counter()
            if sleep_for > 0:
                time.sleep(sleep_for)


async def send_ws(args: argparse.Namespace) -> None:
    try:
        import websockets  # type: ignore
    except Exception as exc:  # pragma: no cover
        raise RuntimeError(
            "WebSocket mode requires the 'websockets' package. Install with: pip install websockets"
        ) from exc

    uri = f"ws://{args.host}:{args.port}"
    interval = 1.0 / args.hz
    sent = 0
    start = time.perf_counter()

    async with websockets.connect(uri, ping_interval=20, ping_timeout=20) as ws:
        while args.count == 0 or sent < args.count:
            now = time.perf_counter() - start
            value = make_value(
                args.pattern,
                now,
                args.amplitude,
                args.offset,
                args.sine_period,
                args.ramp_period,
            )
            await ws.send(f"{value:.6f}")
            sent += 1

            next_deadline = start + sent * interval
            sleep_for = next_deadline - time.perf_counter()
            if sleep_for > 0:
                await asyncio.sleep(sleep_for)


def main() -> int:
    args = parse_args()

    if args.port < 1 or args.port > 65535:
        print("Error: --port must be in range 1..65535", file=sys.stderr)
        return 2

    if args.hz <= 0:
        print("Error: --hz must be > 0", file=sys.stderr)
        return 2

    if args.mode == "udp":
        send_udp(args)
        return 0

    try:
        asyncio.run(send_ws(args))
    except RuntimeError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
