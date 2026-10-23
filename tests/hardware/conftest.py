"""Fixtures for the hardware tests; the ports, GNSS and IMU fixtures come from tests/conftest.py."""

import os
import time

import pytest
from hwutil import _join_wifi, _run, _wifi_connect_command


@pytest.fixture(scope="session", autouse=True)
def reset_modules(app_exe):
    """Reboot every module once before the run so no GATT server, socket or link survives an earlier session."""
    ports = [os.environ.get(v) for v in ("UCX_PORT_A", "UCX_PORT_B")]
    for port in filter(None, ports):
        out = _run(app_exe, port, "device.reboot", timeout=60)
        assert "module rebooted" in out.stdout, f"reset of {port} failed:\n{out.stdout}"
    if any(ports):
        time.sleep(5)


@pytest.fixture(scope="session")
def wifi_peers(app_exe, port_a, port_b):
    """Both modules joined to the Wi-Fi: {port: (ip, gateway)}."""
    connect = _wifi_connect_command()
    return {port: _join_wifi(app_exe, port, connect) for port in (port_a, port_b)}
