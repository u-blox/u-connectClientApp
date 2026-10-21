"""Shared fixtures for tests that run the real ucxapp executable.

The tests use whatever is attached: one NORA module runs the single-module tests, a second one adds the
two-module tests, and a GNSS receiver or IMU adds the tests and bridges that need it.

Environment (an empty value means "not attached", as set by `ucxapp --detect --test`):
  UCX_APP_EXE   path to the exe (default: app/bin/ucxapp.exe, app/bin/ucxapp on Linux)
  UCX_PORT_A    COM port of the first NORA-W36 (default: first FTDI port found)
  UCX_PORT_B    COM port of the second NORA-W36 (default: second FTDI port found)
  UCX_GNSS_PORT, UCX_GNSS_BAUD   GNSS receiver (default: what gnss.detect finds)
  UCX_IMU_PORT, UCX_IMU_BAUD     IMU (default: what imu.detect finds)
"""

import functools
import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = ROOT / "app" / "bin" / ("ucxapp.exe" if sys.platform == "win32" else "ucxapp")
HARDWARE_MARKERS = ("hardware", "two_devices", "gnss", "imu")


def _exe():
    return Path(os.environ.get("UCX_APP_EXE", DEFAULT_EXE))


@functools.lru_cache(maxsize=1)
def _detected():
    if sys.platform != "win32":
        return []
    sys.path.insert(0, str(ROOT / "tests"))
    from harness.portenum import find_at_ports

    return find_at_ports(_exe())


@functools.lru_cache(maxsize=1)
def _ports():
    if "UCX_PORT_A" in os.environ:  # set, even empty: never probe the serial ports (probing can block)
        return os.environ["UCX_PORT_A"] or None, os.environ.get("UCX_PORT_B") or None
    found = _detected()
    return (found[0] if found else None), (found[1] if len(found) > 1 else None)


@functools.lru_cache(maxsize=None)
def _source(name):
    """(port, baud) of the GNSS receiver or IMU, or None."""
    key = f"UCX_{name.upper()}_PORT"
    if key in os.environ:
        port = os.environ[key]
        return (port, os.environ.get(f"UCX_{name.upper()}_BAUD", "")) if port else None
    sys.path.insert(0, str(ROOT))
    import ucxapp

    found = (ucxapp.find_gnss if name == "gnss" else ucxapp.find_imu)(_exe())
    return found[:2] if found else None


def pytest_configure(config):
    for marker in (
        "app: runs the built ucxapp.exe (Windows only, no hardware needed)",
        "hardware: needs a NORA-W36 on UCX_PORT_A",
        "two_devices: needs NORA-W36 modules on both UCX_PORT_A and UCX_PORT_B",
        "gnss: needs a GNSS receiver (UCX_GNSS_PORT or gnss.detect)",
        "imu: needs an IMU (UCX_IMU_PORT or imu.detect)",
    ):
        config.addinivalue_line("markers", marker)


def _skip_reason(item, exe, hw):
    needs_app = any(item.get_closest_marker(m) for m in ("app", *HARDWARE_MARKERS))
    if not needs_app:
        return None
    if sys.platform != "win32" and not (item.get_closest_marker("app") and exe.is_file()):
        return "needs Windows"
    if not exe.is_file():
        return f"app not built: {exe}"
    if (item.get_closest_marker("hardware") or item.get_closest_marker("two_devices")) and not hw["a"]:
        return "no NORA module found (set UCX_PORT_A)"
    if item.get_closest_marker("two_devices") and not hw["b"]:
        return "only one NORA module found (set UCX_PORT_B)"
    for name in ("gnss", "imu"):
        if item.get_closest_marker(name) and not hw[name]:
            return f"no {name.upper()} found (set UCX_{name.upper()}_PORT)"
    return None


@pytest.hookimpl(trylast=True)  # after -k/-m deselection, so only the selected tests trigger detection
def pytest_collection_modifyitems(config, items):
    exe = _exe()
    wanted = {m for item in items for m in HARDWARE_MARKERS if item.get_closest_marker(m)}
    hw = {"a": None, "b": None, "gnss": None, "imu": None}
    if wanted and exe.is_file() and sys.platform == "win32":
        hw["a"], hw["b"] = _ports()
        for name in ("gnss", "imu"):
            if name in wanted:
                hw[name] = _source(name)

    run, skipped = 0, {}
    for item in items:
        reason = _skip_reason(item, exe, hw)
        if reason:
            item.add_marker(pytest.mark.skip(reason=reason))
            skipped[reason] = skipped.get(reason, 0) + 1
        elif any(item.get_closest_marker(m) for m in HARDWARE_MARKERS):
            run += 1

    reporter = config.pluginmanager.get_plugin("terminalreporter")
    if not wanted or reporter is None:
        return
    modules = ", ".join(p for p in (hw["a"], hw["b"]) if p) or "none"
    gnss = f"{hw['gnss'][0]} at {hw['gnss'][1]} baud" if hw["gnss"] else "none"
    imu = f"{hw['imu'][0]} at {hw['imu'][1]} baud" if hw["imu"] else "none"
    reporter.write_line(f"Found: NORA {modules} | GNSS {gnss} | IMU {imu}", bold=True)
    reporter.write_line(f"Starting {run} hardware test(s) using what was found")
    for reason, count in skipped.items():
        reporter.write_line(f"  skipping {count}: {reason}")


@pytest.fixture(scope="session")
def app_exe():
    return _exe()


@pytest.fixture(scope="session")
def port_a():
    return _ports()[0]


@pytest.fixture(scope="session")
def port_b():
    return _ports()[1]


@pytest.fixture(scope="session")
def gnss():
    return _source("gnss")


@pytest.fixture(scope="session")
def imu():
    return _source("imu")

