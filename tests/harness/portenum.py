"""Find the AT-channel COM ports of attached u-blox EVKs (adapted from ucx-test-app/helpers/portenum.py)."""

import subprocess
from pathlib import Path

FTDI_VID = 0x0403
UBLOX_VID = 0x1546
# FT2232H, FT4232H, FT232H, FT4222H, FT231X
FTDI_PIDS = {0x6010, 0x6011, 0x6014, 0x601C, 0x6015}
# The FT4232H driver appends A/B/C/D to the serial number; channel A is the AT port.
LOG_SUFFIXES = {"B", "C", "D"}


def _from_pyserial():
    from serial.tools import list_ports

    found = []
    for p in list_ports.comports():
        if p.vid is None:
            continue
        ours = p.vid == UBLOX_VID or (p.vid == FTDI_VID and p.pid in FTDI_PIDS)
        if not ours:
            continue
        suffix = (p.serial_number or "")[-1:].upper()
        if p.pid == 0x6011 and suffix in LOG_SUFFIXES:
            continue
        found.append(p.device)
    return sorted(found, key=lambda d: int(d[3:]) if d[3:].isdigit() else 0)


def _from_app(exe):
    out = subprocess.run([str(exe), "--list-modules"], capture_output=True, text=True, timeout=30).stdout
    return [line.strip() for line in out.splitlines() if line.strip().startswith("COM")]


def find_at_ports(exe=None):
    """AT ports via the app's own --list-modules, else via pyserial when installed."""
    if exe is not None and Path(exe).is_file():
        try:
            found = _from_app(exe)
            if found:
                return found
        except (OSError, subprocess.SubprocessError):
            pass
    try:
        return _from_pyserial()
    except ImportError:
        return []
