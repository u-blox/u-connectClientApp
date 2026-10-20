"""Smoke tests that need the built exe but no hardware."""

import re
import subprocess

import pytest

pytestmark = pytest.mark.app


def test_version_flag(app_exe):
    out = subprocess.run([str(app_exe), "--version"], capture_output=True, text=True, timeout=10, check=True)
    assert re.fullmatch(r"3\.4\.0\.\d{6}", out.stdout.strip())


def test_list_ports_exits_cleanly(app_exe):
    out = subprocess.run([str(app_exe), "--list-ports"], capture_output=True, text=True, timeout=30, errors="replace")
    assert out.returncode == 0


def test_shell_starts_without_device_and_quits(app_exe):
    out = subprocess.run([str(app_exe), "COM255", "--shell"], input="help\nquit\n", capture_output=True, text=True,
                         timeout=30, errors="replace")
    assert out.returncode == 0
    assert "Command shell" in out.stdout
    assert "hid.setup" in out.stdout
