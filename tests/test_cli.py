"""Command-layer CLI tests: u-connectClientApp.exe <port> --exec "<command>" ..."""

import subprocess

import pytest

pytestmark = pytest.mark.app


def _run(app_exe, tmp_path, port, *commands, timeout=60):
    args = [str(app_exe), port, "--ini", str(tmp_path / "cli.ini")]
    for command in commands:
        args += ["--exec", command]
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout, errors="replace")


def test_help_lists_command_packs(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "help")
    assert out.returncode == 0
    for name in ("device.info", "wifi.scan", "wifi.connect", "sock.open", "device.connect",
                 "net.ping", "net.dns", "http.get", "mqtt.connect", "mqtt.publish",
                 "bt.scan", "bt.connect", "at"):
        assert name in out.stdout


def test_unknown_command_fails(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "no.such.command")
    assert out.returncode == 1
    assert "unknown command" in out.stdout


def test_device_command_without_device_fails(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "wifi.status")
    assert out.returncode == 1
    assert "not connected" in out.stdout


def test_usage_is_printed_for_bad_arguments(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "device.connect a b")
    assert out.returncode == 1
    assert "usage: device.connect" in out.stdout


@pytest.mark.hardware
def test_device_info_and_wifi_status(app_exe, tmp_path, port_a):
    out = _run(app_exe, tmp_path, port_a, "device.info", "wifi.status", "sock.list")
    assert out.returncode == 0, out.stdout
    assert "Model:" in out.stdout
    assert "NORA-W36" in out.stdout
    assert "wifi:" in out.stdout
    assert "socket(s)" in out.stdout
