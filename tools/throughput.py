# pyright: strict
# SPDX-FileCopyrightText: 2026 Gustavo Franco
# SPDX-License-Identifier: MIT

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass

TIMEOUT_S = 60
KIB = 1024


class ThroughputError(Exception):
    pass


@dataclass(frozen=True, slots=True)
class Measurement:
    requests: int
    bytes: int
    seconds: float

    @property
    def kib_per_second(self) -> float:
        return (self.bytes / KIB / self.seconds) if self.seconds > 0 else 0.0

    def report(self) -> dict[str, float | int]:
        return {
            "requests": self.requests,
            "bytes": self.bytes,
            "seconds": round(self.seconds, 4),
            "kib_per_second": round(self.kib_per_second, 2),
        }


def fetch(request: urllib.request.Request) -> bytes:
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_S) as response:
            return response.read()
    except urllib.error.HTTPError as error:
        raise ThroughputError(f"{request.full_url} answered {error.code}") from error
    except urllib.error.URLError as error:
        raise ThroughputError(
            f"{request.full_url} could not be reached: {error.reason}"
        ) from error


def measure_download(url: str, count: int) -> Measurement:
    total = 0
    started = time.perf_counter()
    for _ in range(count):
        request = urllib.request.Request(url, headers={"Accept-Encoding": "gzip"})
        total += len(fetch(request))
    return Measurement(count, total, time.perf_counter() - started)


def measure_upload(url: str, size: int) -> Measurement:
    payload = os.urandom(size)
    request = urllib.request.Request(
        url,
        data=payload,
        method="PUT",
        headers={"Content-Type": "application/octet-stream"},
    )
    started = time.perf_counter()
    fetch(request)
    return Measurement(1, size, time.perf_counter() - started)


def parser() -> argparse.ArgumentParser:
    command = argparse.ArgumentParser(description="Measure download and upload speed.")
    command.add_argument("--download", required=True, help="URL fetched repeatedly")
    command.add_argument("--count", type=int, default=10, help="downloads to time")
    command.add_argument("--upload", help="URL that takes a PUT, such as RestFiles")
    command.add_argument("--upload-bytes", type=int, default=65536, help="upload size")
    return command


def main(arguments: list[str]) -> int:
    options = parser().parse_args(arguments)
    try:
        report = {
            "download": measure_download(options.download, options.count).report()
        }
        if options.upload:
            report["upload"] = measure_upload(
                options.upload, options.upload_bytes
            ).report()
    except ThroughputError as error:
        print(f"throughput: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
