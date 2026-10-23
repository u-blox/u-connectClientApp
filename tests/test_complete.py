"""Tab completion (app.complete uses the same code as Tab in the TUI), offline."""

import subprocess

import pytest

pytestmark = pytest.mark.app


def _complete(app_exe, tmp_path, prefix):
    out = subprocess.run([str(app_exe), "--offline", "--exec", f"app.complete {prefix}"], capture_output=True,
                         text=True, timeout=60, errors="replace", cwd=tmp_path, stdin=subprocess.DEVNULL)
    return out.stdout


@pytest.mark.parametrize("prefix, expected", [
    ("app.compl", "1 match: app.complete"),
    ("gatt.client.hr", "1 match: gatt.client.hrs"),
    ("gatt.server.servicech", "1 match: gatt.server.servicechanged"),
    ("gatt.server.wp", "1 match: gatt.server.wps"),
    ("utils.unz", "1 match: utils.unzip"),
])
def test_unique_prefix_completes(app_exe, tmp_path, prefix, expected):
    assert expected in _complete(app_exe, tmp_path, prefix)


def test_common_prefix(app_exe, tmp_path):
    out = _complete(app_exe, tmp_path, "gatt.cl")
    assert "matches: gatt.client." in out


def test_no_example_names_left(app_exe, tmp_path):
    assert "0 matches" in _complete(app_exe, tmp_path, "gatt.client.example")
    assert "0 matches" in _complete(app_exe, tmp_path, "gatt.server.example")


def test_unknown_prefix(app_exe, tmp_path):
    assert "0 matches: nosuch" in _complete(app_exe, tmp_path, "nosuch")
