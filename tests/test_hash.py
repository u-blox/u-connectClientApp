"""Own md5/sha256 (no mbedtls): known-answer vectors and hash.text, offline."""

import subprocess


def _run(app_exe, cwd, *commands):
    args = [str(app_exe), "--offline"]
    for command in commands:
        args += ["--exec", command]
    return subprocess.run(args, capture_output=True, text=True, timeout=60, errors="replace", cwd=cwd,
                          stdin=subprocess.DEVNULL)


def test_hash_selftest(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "hash.selftest")
    assert out.returncode == 0, out.stdout + out.stderr
    assert "hash selftest: md5 and sha256 ok" in out.stdout


def test_hash_text(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "hash.text sha256 abc", "hash.text md5 abc")
    assert "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" in out.stdout
    assert "900150983cd24fb0d6963f7d28e17f72" in out.stdout


def test_sock_echo_needs_a_device(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "sock.echo udp 7")
    assert "not connected to a module" in out.stdout
