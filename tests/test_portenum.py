"""Unit tests for the COM-port discovery helper (no hardware needed)."""

import subprocess
import sys
import types

import pytest
from harness import portenum


def _port(device, vid, pid=None, serial=None):
    return types.SimpleNamespace(device=device, vid=vid, pid=pid, serial_number=serial)


def _install_ports(monkeypatch, ports):
    serial_mod = types.ModuleType("serial")
    tools_mod = types.ModuleType("serial.tools")
    list_ports_mod = types.ModuleType("serial.tools.list_ports")
    list_ports_mod.comports = lambda: ports
    serial_mod.tools = tools_mod
    tools_mod.list_ports = list_ports_mod
    monkeypatch.setitem(sys.modules, "serial", serial_mod)
    monkeypatch.setitem(sys.modules, "serial.tools", tools_mod)
    monkeypatch.setitem(sys.modules, "serial.tools.list_ports", list_ports_mod)


def test_pyserial_keeps_only_ublox_and_ftdi_at_ports_sorted(monkeypatch):
    _install_ports(monkeypatch, [
        _port("COM12", portenum.UBLOX_VID),
        _port("COM3", portenum.FTDI_VID, 0x6011, "ABC123A"),
        _port("COM4", portenum.FTDI_VID, 0x6011, "ABC123B"),   # FT4232H log channel
        _port("COM5", portenum.FTDI_VID, 0x6014, "XYZ"),
        _port("COM6", 0x1234, 0x0001),                         # someone else's device
        _port("COM7", None),                                   # no USB ids
    ])
    assert portenum._from_pyserial() == ["COM3", "COM5", "COM12"]


def test_from_app_parses_list_modules_output(monkeypatch):
    out = subprocess.CompletedProcess([], 0, stdout="Modules:\n  COM13\nnoise\nCOM155\n", stderr="")
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: out)
    assert portenum._from_app("any.exe") == ["COM13", "COM155"]


def test_find_at_ports_prefers_the_app(monkeypatch, tmp_path):
    exe = tmp_path / "app.exe"
    exe.write_text("")
    monkeypatch.setattr(portenum, "_from_app", lambda e: ["COM9"])
    monkeypatch.setattr(portenum, "_from_pyserial", lambda: pytest.fail("pyserial must not be used"))
    assert portenum.find_at_ports(exe) == ["COM9"]


def test_find_at_ports_falls_back_to_pyserial(monkeypatch, tmp_path):
    exe = tmp_path / "app.exe"
    exe.write_text("")

    def broken(_exe):
        raise OSError("cannot start")

    monkeypatch.setattr(portenum, "_from_app", broken)
    monkeypatch.setattr(portenum, "_from_pyserial", lambda: ["COM4"])
    assert portenum.find_at_ports(exe) == ["COM4"]
    monkeypatch.setattr(portenum, "_from_app", lambda e: [])
    assert portenum.find_at_ports(exe) == ["COM4"]
    assert portenum.find_at_ports(None) == ["COM4"]


def test_find_at_ports_without_pyserial_returns_nothing(monkeypatch):
    def missing():
        raise ImportError("no pyserial")

    monkeypatch.setattr(portenum, "_from_pyserial", missing)
    assert portenum.find_at_ports(None) == []
