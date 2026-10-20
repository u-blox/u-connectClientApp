"""Shared fixtures for tests that run the real u-connectClientApp.exe.

Environment:
  UCX_APP_EXE   path to the exe (default: u-connectClientApp/bin/u-connectClientApp.exe)
  UCX_PORT_A    COM port of the first NORA-W36 (default: first FTDI port found)
  UCX_PORT_B    COM port of the second NORA-W36 (default: second FTDI port found)
"""

import functools
import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = ROOT / "u-connectClientApp" / "bin" / ("u-connectClientApp.exe" if sys.platform == "win32" else "ucxapp")


@functools.lru_cache(maxsize=1)
def _detected():
    if sys.platform != "win32":
        return []
    sys.path.insert(0, str(ROOT / "tests"))
    from harness.portenum import find_at_ports

    return find_at_ports(os.environ.get("UCX_APP_EXE", DEFAULT_EXE))


def _ports():
    found = _detected()
    a = os.environ.get("UCX_PORT_A") or (found[0] if found else None)
    b = os.environ.get("UCX_PORT_B") or (found[1] if len(found) > 1 else None)
    return a, b


def pytest_configure(config):
    for marker in (
        "app: runs the built u-connectClientApp.exe (Windows only, no hardware needed)",
        "hardware: needs a NORA-W36 on UCX_PORT_A",
        "two_devices: needs NORA-W36 modules on both UCX_PORT_A and UCX_PORT_B",
    ):
        config.addinivalue_line("markers", marker)


def pytest_collection_modifyitems(config, items):
    exe = Path(os.environ.get("UCX_APP_EXE", DEFAULT_EXE))
    port_a, port_b = _ports()
    for item in items:
        needs_app = any(item.get_closest_marker(m) for m in ("app", "hardware", "two_devices"))
        if needs_app and sys.platform != "win32" and not (item.get_closest_marker("app") and exe.is_file()):
            item.add_marker(pytest.mark.skip(reason="needs Windows"))
        elif needs_app and not exe.is_file():
            item.add_marker(pytest.mark.skip(reason=f"app not built: {exe}"))
        elif item.get_closest_marker("hardware") and not port_a:
            item.add_marker(pytest.mark.skip(reason="set UCX_PORT_A to run hardware tests"))
        elif item.get_closest_marker("two_devices") and not (port_a and port_b):
            item.add_marker(pytest.mark.skip(reason="set UCX_PORT_A and UCX_PORT_B to run two-device tests"))


@pytest.fixture(scope="session")
def app_exe():
    return Path(os.environ.get("UCX_APP_EXE", DEFAULT_EXE))


@pytest.fixture(scope="session")
def port_a():
    return _ports()[0]


@pytest.fixture(scope="session")
def port_b():
    return _ports()[1]

