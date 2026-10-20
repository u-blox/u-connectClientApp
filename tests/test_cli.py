"""Command-layer CLI tests: u-connectClientApp.exe <port> --exec "<command>" ..."""

import subprocess

import pytest

pytestmark = pytest.mark.app


def _run(app_exe, tmp_path, port, *commands, timeout=60):
    args = [str(app_exe), port]
    for command in commands:
        args += ["--exec", command]
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout, errors="replace")


def test_help_lists_command_packs(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "help")
    assert out.returncode == 0
    for name in ("device.info", "wifi.scan", "wifi.connect", "sock.open", "device.connect",
                 "net.ping", "net.dns", "http.get", "mqtt.connect", "mqtt.publish",
                 "bt.scan", "bt.connect", "at"):
        assert name in out.stdout


def test_unknown_command_fails(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "no.such.command")
    assert out.returncode == 1
    assert "unknown command" in out.stdout


def test_device_command_without_device_fails(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "wifi.status")
    assert out.returncode == 1
    assert "not connected" in out.stdout


def test_usage_is_printed_for_bad_arguments(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "device.connect a b")
    assert out.returncode == 1
    assert "usage: device.connect" in out.stdout


@pytest.mark.parametrize("command", ["web.start", "hid.status", "hid.key a", "wifi.ap.status", "gatt.writelong 0 1 00"])
def test_new_device_commands_need_a_device(app_exe, tmp_path, command):
    out = _run(app_exe, tmp_path, "COM255", command)
    assert out.returncode == 1
    assert "not connected" in out.stdout


@pytest.mark.parametrize("command", [
    "mqtt.connect", "mqtt.disconnect", "mqtt.subscribe t", "mqtt.unsubscribe t",
    "mqtt.publish t hello", "mqtt.read 0",
])
def test_mqtt_commands_need_a_device(app_exe, tmp_path, command):
    out = _run(app_exe, tmp_path, "COM255", command)
    assert out.returncode == 1
    assert "not connected" in out.stdout


@pytest.mark.parametrize("command", ["http.get", "http.download http://x", "http.get " + "a" * 300])
def test_http_commands_reject_bad_arguments(app_exe, tmp_path, command):
    out = _run(app_exe, tmp_path, "COM255", command)
    assert out.returncode == 1


def test_web_stop_and_status_work_without_device(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "web.status", "web.stop")
    assert out.returncode == 0, out.stdout
    assert "not running" in out.stdout


def test_qr_text_needs_no_device(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "qr.text hello", "qr.wifi stenmo secret", "qr.geo 59.9 10.7")
    assert out.returncode == 0, out.stdout


def test_file_hash_sha256(app_exe, tmp_path):
    f = tmp_path / "abc.txt"
    f.write_bytes(b"abc")
    out = _run(app_exe, tmp_path, "COM255", f"file.hash sha256 {f}")
    assert out.returncode == 0, out.stdout
    assert "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" in out.stdout.lower()


def test_file_hash_md5_and_missing_file(app_exe, tmp_path):
    f = tmp_path / "abc.txt"
    f.write_bytes(b"abc")
    out = _run(app_exe, tmp_path, "COM255", f"file.hash md5 {f}")
    assert out.returncode == 0, out.stdout
    assert "900150983cd24fb0d6963f7d28e17f72" in out.stdout.lower()
    out = _run(app_exe, tmp_path, "COM255", f"file.hash md5 {tmp_path / 'missing.bin'}")
    assert out.returncode == 1


def test_help_prefix_filters_commands(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "help wifi.profile")
    assert out.returncode == 0
    assert "wifi.profile.auto" in out.stdout
    assert "sock.open" not in out.stdout


def test_wifi_profiles_round_trip(app_exe, tmp_path):
    def run(*commands):
        args = [str(app_exe), "COM255"]
        for command in commands:
            args += ["--exec", command]
        return subprocess.run(args, capture_output=True, text=True, timeout=60, errors="replace", cwd=tmp_path)

    out = run("wifi.profile.add home ubx s3cret!", "wifi.profile.add cafe CafeNet", "wifi.profile.list")
    assert out.returncode == 0, out.stdout
    assert "home: ssid=ubx WPA" in out.stdout
    assert "cafe: ssid=CafeNet open" in out.stdout
    assert "2 profile(s)" in out.stdout
    assert "s3cret!" not in out.stdout
    assert "s3cret!" not in (tmp_path / "u-connectClientApp.wifi").read_text()

    out = run("wifi.profile.add home ubx newpass", "wifi.profile.remove cafe", "wifi.profile.list")
    assert "1 profile(s)" in out.stdout

    out = run("wifi.profile.remove nosuch")
    assert out.returncode == 1

    out = run("wifi.profile.connect home")
    assert out.returncode == 1
    assert "not connected" in out.stdout


def test_hid_key_usage_error(app_exe, tmp_path):
    out = _run(app_exe, tmp_path, "COM255", "hid.key")
    assert out.returncode == 1


@pytest.mark.hardware
def test_device_info_and_wifi_status(app_exe, tmp_path, port_a):
    out = _run(app_exe, tmp_path, port_a, "device.info", "wifi.status", "sock.list")
    assert out.returncode == 0, out.stdout
    assert "Model:" in out.stdout
    assert "NORA-W36" in out.stdout
    assert "wifi:" in out.stdout
    assert "socket(s)" in out.stdout
