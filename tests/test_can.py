"""CAN (can.*): record format, emulated loopback controller and bridge argument checks; no module needed."""

import subprocess

import pytest


def _run_in(app_exe, cwd, *commands):
    args = [str(app_exe), "--offline"]
    for command in commands:
        args += ["--exec", command]
    return subprocess.run(args, capture_output=True, text=True, timeout=120, errors="replace", cwd=cwd,
                          stdin=subprocess.DEVNULL)


def test_can_selftest_record_format(app_exe, tmp_path):
    out = _run_in(app_exe, tmp_path, "can.selftest")
    assert out.returncode == 0, out.stdout + out.stderr
    assert "can selftest: record format ok" in out.stdout


def test_can_loopback_send_recv_status(app_exe, tmp_path):
    out = _run_in(app_exe, tmp_path, "can.cfg 250000", "can.send 0x123 DE AD BE EF", "can.recv", "can.recv",
                  "can.status", "can.bridge status")
    assert out.returncode == 0, out.stdout
    assert "can bitrate 250000" in out.stdout
    assert "0x123 [4] DE AD BE EF" in out.stdout
    assert "no frame" in out.stdout
    assert "250000 bit/s, tx 1, rx 1, queued 0" in out.stdout
    assert "bridge: off" in out.stdout


def test_hw_has_no_can_commands(app_exe, tmp_path):
    out = _run_in(app_exe, tmp_path, "hw.exec can.send 0x1")
    assert "unknown command" in out.stdout


@pytest.mark.parametrize("command", ["can.send 0x20000000", "can.send 0x1 112233445566778899", "can.send 0x1 zz",
                                     "can.cfg 5", "can.recv x", "can.bridge"])
def test_can_usage(app_exe, tmp_path, command):
    out = _run_in(app_exe, tmp_path, command)
    assert out.returncode == 1
    assert "usage" in out.stdout.lower()
