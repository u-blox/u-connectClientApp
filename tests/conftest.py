"""Shared fixtures for tests that run the real ucx-windows-app.exe.

Environment:
  UCX_APP_EXE   path to the exe (default: ucx-windows-app/bin/ucx-windows-app.exe)
  UCX_PORT_A    COM port of the first NORA-W36 (enables hardware tests)
  UCX_PORT_B    COM port of the second NORA-W36 (enables two-device tests)
"""

import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = ROOT / "ucx-windows-app" / "bin" / "ucx-windows-app.exe"


def pytest_configure(config):
    for marker in (
        "app: runs the built ucx-windows-app.exe (Windows only, no hardware needed)",
        "hardware: needs a NORA-W36 on UCX_PORT_A",
        "two_devices: needs NORA-W36 modules on both UCX_PORT_A and UCX_PORT_B",
        "pc_ble: needs a NORA-W36 on UCX_PORT_A plus the PC's built-in Bluetooth LE radio",
    ):
        config.addinivalue_line("markers", marker)


def pytest_collection_modifyitems(config, items):
    exe = Path(os.environ.get("UCX_APP_EXE", DEFAULT_EXE))
    port_a, port_b = os.environ.get("UCX_PORT_A"), os.environ.get("UCX_PORT_B")
    for item in items:
        needs_app = any(item.get_closest_marker(m) for m in ("app", "hardware", "two_devices", "pc_ble"))
        if needs_app and sys.platform != "win32":
            item.add_marker(pytest.mark.skip(reason="needs Windows"))
        elif needs_app and not exe.is_file():
            item.add_marker(pytest.mark.skip(reason=f"app not built: {exe}"))
        elif item.get_closest_marker("hardware") and not port_a:
            item.add_marker(pytest.mark.skip(reason="set UCX_PORT_A to run hardware tests"))
        elif item.get_closest_marker("pc_ble") and not port_a:
            item.add_marker(pytest.mark.skip(reason="set UCX_PORT_A to run PC BLE tests"))
        elif item.get_closest_marker("two_devices") and not (port_a and port_b):
            item.add_marker(pytest.mark.skip(reason="set UCX_PORT_A and UCX_PORT_B to run two-device tests"))


@pytest.fixture(scope="session")
def app_exe():
    return Path(os.environ.get("UCX_APP_EXE", DEFAULT_EXE))


@pytest.fixture(scope="session")
def port_a():
    return os.environ.get("UCX_PORT_A")


@pytest.fixture(scope="session")
def port_b():
    return os.environ.get("UCX_PORT_B")


@pytest.fixture
def make_app(app_exe, tmp_path, request):
    """Factory for isolated app instances (own settings file, own transcript in tests/logs)."""
    pytest.importorskip("winpty", reason="pip install -r tests/requirements.txt")
    from harness.ucxapp import UcxApp

    started = []
    log_dir = ROOT / "tests" / "logs" / request.node.name.replace("/", "_")

    def _make(name="app", port=None):
        app = UcxApp(app_exe, port=port, ini=tmp_path / f"{name}.ini", name=name, log_dir=log_dir)
        app.start()
        started.append(app)
        return app

    yield _make
    for app in started:
        app.close()
