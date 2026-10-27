import json
import sys
import threading
from collections.abc import Iterator
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import ClassVar

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.throughput import (
    Measurement,
    ThroughputError,
    main,
    measure_download,
    measure_upload,
)

BODY = bytes(range(256)) * 16


class Handler(BaseHTTPRequestHandler):
    uploads: ClassVar[list[bytes]] = []

    def do_GET(self) -> None:
        if self.path != "/data":
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(BODY)))
        self.end_headers()
        self.wfile.write(BODY)

    def do_PUT(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        Handler.uploads.append(self.rfile.read(length))
        self.send_response(201)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, format: str, *args: object) -> None:
        return


@pytest.fixture
def server() -> Iterator[ThreadingHTTPServer]:
    instance = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=instance.serve_forever, daemon=True)
    thread.start()
    yield instance
    instance.shutdown()
    instance.server_close()


def base_url(instance: ThreadingHTTPServer) -> str:
    return f"http://127.0.0.1:{instance.server_address[1]}"


def test_measures_downloads_by_bytes_and_time(server: ThreadingHTTPServer) -> None:
    # Arrange
    url = base_url(server) + "/data"

    # Act
    result = measure_download(url, 3)

    # Assert
    assert result.requests == 3
    assert result.bytes == 3 * len(BODY)
    assert result.seconds > 0
    assert result.kib_per_second == pytest.approx(result.bytes / 1024 / result.seconds)


def test_measures_an_upload_of_the_given_size(server: ThreadingHTTPServer) -> None:
    # Arrange
    Handler.uploads = []
    url = base_url(server) + "/api/files/BENCH.BIN"

    # Act
    result = measure_upload(url, 5000)

    # Assert
    assert result.requests == 1
    assert result.bytes == 5000
    assert [len(upload) for upload in Handler.uploads] == [5000]


def test_refuses_a_failed_request(server: ThreadingHTTPServer) -> None:
    # Arrange
    url = base_url(server) + "/missing"

    # Act
    with pytest.raises(ThroughputError) as caught:
        measure_download(url, 1)

    # Assert
    assert "404" in str(caught.value)


def test_prints_a_json_summary(
    server: ThreadingHTTPServer, capsys: pytest.CaptureFixture[str]
) -> None:
    # Arrange
    arguments = ["--download", base_url(server) + "/data", "--count", "2"]

    # Act
    status = main(arguments)

    # Assert
    report = json.loads(capsys.readouterr().out)
    assert status == 0
    assert report["download"]["requests"] == 2
    assert report["download"]["bytes"] == 2 * len(BODY)
    assert "upload" not in report


def test_measurement_reports_zero_speed_for_zero_time() -> None:
    # Arrange
    measurement = Measurement(1, 10, 0.0)

    # Act
    speed = measurement.kib_per_second

    # Assert
    assert speed == 0.0
