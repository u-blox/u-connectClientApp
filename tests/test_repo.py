"""Repository sanity checks that don't need a compiler or hardware."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def test_submodule_is_u_connect_client():
    text = (ROOT / ".gitmodules").read_text(encoding="utf-8")
    assert "path = u-connectClient" in text
    assert "u-blox/u-connectClient.git" in text
    assert "ucxclient" not in text


def test_cmake_references_existing_submodule_files():
    cmake = (ROOT / "u-connectClientApp" / "CMakeLists.txt").read_text(encoding="utf-8")
    for rel in re.findall(r"\.\./(u-connectClient/[\w./-]+\.(?:cmake|c))\b", cmake):
        assert (ROOT / rel).is_file(), f"{rel} referenced by CMakeLists.txt is missing (submodule not initialised?)"


def test_no_stale_ucxclient_paths_in_build_files():
    for rel in ("u-connectClientApp.cmd", "u-connectClientApp/CMakeLists.txt"):
        text = (ROOT / rel).read_text(encoding="utf-8")
        assert not re.search(r"(?<![\w-])ucxclient[\\/]", text), f"{rel} still references ucxclient/"


def test_cmd_files_use_crlf():
    for cmd in ROOT.glob("*.cmd"):
        data = cmd.read_bytes()
        assert b"\n" not in data.replace(b"\r\n", b""), f"{cmd.name} has LF-only line endings"
