"""CoAP over UDP on the host (coap.*) and NTP client (net.ntp): offline checks, no module needed."""

import subprocess

import pytest


def _run_in(app_exe, cwd, *commands):
    args = [str(app_exe), "--offline"]
    for command in commands:
        args += ["--exec", command]
    return subprocess.run(args, capture_output=True, text=True, timeout=120, errors="replace", cwd=cwd,
                          stdin=subprocess.DEVNULL)


@pytest.fixture
def work_dir(tmp_path):
    return tmp_path


def test_coap_selftest_encodes_and_parses(app_exe, work_dir):
    out = _run_in(app_exe, work_dir, "coap.selftest")
    assert out.returncode == 0, out.stdout + out.stderr
    assert "coap selftest: GET encoding and 2.05 response parsing ok" in out.stdout


def test_coap_get_needs_a_device(app_exe, work_dir):
    out = _run_in(app_exe, work_dir, "coap.get 127.0.0.1 5683 /")
    assert "not connected to a module" in out.stdout


def test_coap_get_checks_its_arguments_after_the_device(app_exe, work_dir):
    out = _run_in(app_exe, work_dir, "coap.get")
    assert "not connected to a module" in out.stdout or "usage" in out.stdout.lower()


def test_ntp_needs_a_device(app_exe, work_dir):
    out = _run_in(app_exe, work_dir, "net.ntp status")
    assert "not connected to a module" in out.stdout
