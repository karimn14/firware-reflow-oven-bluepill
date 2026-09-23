#!/usr/bin/env python3
"""Record read-only Blue Pill $STAT frames for external PCB profiling."""

import argparse
import csv
from datetime import datetime, timezone
from pathlib import Path
import sys
import time


FIELDS = (
    "host_utc",
    "host_elapsed_ms",
    "stm_up_ms",
    "conveyor_state",
    "thermal_stage",
    "plate_c",
    "setpoint_c",
    "heater_pct",
    "fan_pct",
)


def parse_stat(raw: bytes) -> dict[str, str] | None:
    """Return validated STAT fields; ignore prompts, CSV, and broken frames."""
    try:
        line = raw.decode("ascii").strip()
    except UnicodeDecodeError:
        return None
    if not line.startswith("$STAT,") or len(line) < 10:
        return None
    body, separator, checksum = line[1:].rpartition("*")
    if not separator or len(checksum) != 2:
        return None
    try:
        expected = int(checksum, 16)
    except ValueError:
        return None
    actual = 0
    for byte in body.encode("ascii"):
        actual ^= byte
    if actual != expected:
        return None

    parts = body.split(",")
    fields = {}
    for part in parts[1:]:
        key, separator, value = part.partition("=")
        if not separator or not key or key in fields:
            return None
        fields[key] = value
    if not fields.get("up", "").isdigit():
        return None
    return fields


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="USB CDC port, for example /dev/ttyACM0")
    parser.add_argument("output", type=Path, help="CSV output path")
    parser.add_argument("--seconds", type=float, default=0,
                        help="recording duration; 0 means until Ctrl-C")
    args = parser.parse_args()
    if args.seconds < 0:
        parser.error("--seconds must be zero or greater")

    try:
        import serial
    except ImportError:
        print("Install pyserial first: python3 -m pip install pyserial", file=sys.stderr)
        return 2

    started = time.monotonic()
    count = 0
    try:
        with serial.Serial(args.port, 115200, timeout=0.5) as connection, \
                args.output.open("w", newline="", encoding="utf-8") as output:
            writer = csv.DictWriter(output, fieldnames=FIELDS)
            writer.writeheader()
            while args.seconds == 0 or time.monotonic() - started < args.seconds:
                frame = parse_stat(connection.readline())
                if frame is None:
                    continue
                writer.writerow({
                    "host_utc": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                    "host_elapsed_ms": round((time.monotonic() - started) * 1000),
                    "stm_up_ms": frame["up"],
                    "conveyor_state": frame.get("st", ""),
                    "thermal_stage": frame.get("zone", ""),
                    "plate_c": frame.get("pv", ""),
                    "setpoint_c": frame.get("sp", ""),
                    "heater_pct": frame.get("heat", ""),
                    "fan_pct": frame.get("fan", ""),
                })
                output.flush()
                count += 1
    except KeyboardInterrupt:
        pass
    except (OSError, serial.SerialException) as error:
        print(f"Recording failed: {error}", file=sys.stderr)
        return 1
    print(f"Saved {count} plate samples to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
