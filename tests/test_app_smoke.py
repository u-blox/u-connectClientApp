"""Smoke tests that need the built exe but no hardware."""

import re
import subprocess

import pytest

pytestmark = pytest.mark.app


def _start_without_device(make_app):
    app = make_app("nodevice", port="COM255")
    app.expect(r"Failed to connect", 45)
    app.acknowledge(15)  # welcome guide
    app.wait_main_menu(15)
    return app


def test_version_flag(app_exe):
    out = subprocess.run([str(app_exe), "--version"], capture_output=True, text=True, timeout=10, check=True)
    assert re.fullmatch(r"3\.4\.0\.\d{6}", out.stdout.strip())


def test_starts_without_device_and_quits(make_app):
    app = _start_without_device(make_app)
    assert app.quit(), "app did not exit after 'q'"
    assert app.exit_code == 0


def test_toggle_logging_without_device(make_app):
    app = _start_without_device(make_app)
    app.expect(r"Choice:", 5)
    app.send("l")
    app.expect(r"Toggle logging: OFF", 10)
    assert app.quit()
