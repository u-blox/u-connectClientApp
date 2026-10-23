"""utils.zip / utils.unzip round trips against Python's zipfile, offline."""

import os
import subprocess
import zipfile

import pytest

pytestmark = pytest.mark.app


def _run(app_exe, cwd, *commands):
    args = [str(app_exe), "--offline"]
    for command in commands:
        args += ["--exec", command]
    return subprocess.run(args, capture_output=True, text=True, timeout=60, errors="replace", cwd=cwd,
                          stdin=subprocess.DEVNULL)


def _payloads():
    return {
        "text.txt": b"hello zip\n" * 500,
        "random.bin": os.urandom(200_000),
        "empty.dat": b"",
    }


def test_app_zip_read_by_python(app_exe, tmp_path):
    files = _payloads()
    for name, data in files.items():
        (tmp_path / name).write_bytes(data)
    out = _run(app_exe, tmp_path, "utils.zip out.zip " + " ".join(files))
    assert out.returncode == 0, out.stdout + out.stderr
    with zipfile.ZipFile(tmp_path / "out.zip") as z:
        assert z.testzip() is None
        for name, data in files.items():
            assert z.read(name) == data


def test_python_zip_extracted_by_app(app_exe, tmp_path):
    files = _payloads()
    files["sub/dir/nested.txt"] = b"nested"
    with zipfile.ZipFile(tmp_path / "in.zip", "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            z.writestr(name, data)
        z.writestr("stored.txt", b"stored", compress_type=zipfile.ZIP_STORED)
    out = _run(app_exe, tmp_path, "utils.unzip in.zip ex")
    assert out.returncode == 0, out.stdout + out.stderr
    for name, data in files.items():
        assert (tmp_path / "ex" / name).read_bytes() == data
    assert (tmp_path / "ex" / "stored.txt").read_bytes() == b"stored"


def test_round_trip(app_exe, tmp_path):
    (tmp_path / "a.bin").write_bytes(os.urandom(50_000) + b"A" * 50_000)
    out = _run(app_exe, tmp_path, "utils.zip rt.zip a.bin", "utils.unzip rt.zip back")
    assert out.returncode == 0, out.stdout + out.stderr
    assert (tmp_path / "back" / "a.bin").read_bytes() == (tmp_path / "a.bin").read_bytes()


def test_unzip_rejects_unsafe_names(app_exe, tmp_path):
    with zipfile.ZipFile(tmp_path / "evil.zip", "w") as z:
        z.writestr("../evil.txt", b"x")
        z.writestr("ok.txt", b"ok")
    out = _run(app_exe, tmp_path, "utils.unzip evil.zip ex")
    assert "skipped unsafe name" in out.stdout
    assert not (tmp_path / "evil.txt").exists()
    assert (tmp_path / "ex" / "ok.txt").read_bytes() == b"ok"


def test_unzip_bad_input(app_exe, tmp_path):
    (tmp_path / "junk.zip").write_bytes(b"not a zip at all")
    out = _run(app_exe, tmp_path, "utils.unzip junk.zip ex")
    assert out.returncode != 0
    assert "not a zip file" in out.stdout
    out = _run(app_exe, tmp_path, "utils.unzip missing.zip ex")
    assert out.returncode != 0
