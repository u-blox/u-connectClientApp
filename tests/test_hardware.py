"""Tests against real NORA-W36 modules and the PC's built-in Bluetooth LE radio.

Run with e.g.:  set UCX_PORT_A=COM5 & set UCX_PORT_B=COM7 & pytest tests/test_hardware.py
"""

import asyncio
import os
import queue
import re
import socket
import subprocess
import threading
import time
import urllib.error
import urllib.request

import pytest

DEVICE_NAME = "u-blox HID Keyboard"


def _args(exe, port, *commands):
    args = [str(exe), port]
    for command in commands:
        args += ["--exec", command]
    return args


def _run(exe, port, *commands, timeout=120):
    return subprocess.run(_args(exe, port, *commands), capture_output=True, text=True, timeout=timeout,
                          errors="replace")


def _start_server(exe, port):
    return subprocess.Popen(_args(exe, port, "hid.setup", "gatt.listen 40"), stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)


@pytest.mark.hardware
def test_connects_and_reports_model(app_exe, port_a):
    out = _run(app_exe, port_a, "device.info")
    assert out.returncode == 0, out.stdout
    assert "NORA-W36" in out.stdout


@pytest.mark.hardware
def test_wifi_scan_finds_networks(app_exe, port_a):
    out = _run(app_exe, port_a, "wifi.scan")
    assert out.returncode == 0, out.stdout


@pytest.mark.hardware
def test_wifi_profile_auto_falls_back_to_scan(app_exe, port_a, tmp_path):
    ssid, password = os.environ.get("UCX_WIFI_SSID"), os.environ.get("UCX_WIFI_PASS", "")
    if not ssid:
        pytest.skip("set UCX_WIFI_SSID (and UCX_WIFI_PASS) to run the Wi-Fi profile test")
    (tmp_path / "u-connectClientApp.wifilast").write_text("bogus")
    out = subprocess.run(
        _args(app_exe, port_a, "wifi.profile.add bogus no-such-network-xyz pw12345678",
              f"wifi.profile.add real {ssid} {password}", "wifi.profile.auto", "wifi.status"),
        capture_output=True, text=True, timeout=180, errors="replace", cwd=tmp_path)
    assert out.returncode == 0, out.stdout
    assert "Strongest known network: 'real'" in out.stdout, out.stdout
    assert (tmp_path / "u-connectClientApp.wifilast").read_text() == "real"


@pytest.mark.hardware
def test_ble_scan_completes(app_exe, port_a):
    out = _run(app_exe, port_a, "bt.scan")
    assert out.returncode == 0, out.stdout


@pytest.mark.hardware
def test_ble_profile_auto_falls_back_to_scan(app_exe, port_a, tmp_path):
    (tmp_path / "u-connectClientApp.blelast").write_text("ghost")
    out = subprocess.run(
        _args(app_exe, port_a, "bt.profile.add ghost 11:22:33:44:55:66", "bt.profile.list", "bt.profile.auto"),
        capture_output=True, text=True, timeout=180, errors="replace", cwd=tmp_path)
    assert "ghost: 11:22:33:44:55:66" in out.stdout, out.stdout
    assert "Trying last used profile 'ghost'" in out.stdout, out.stdout
    assert "Scanning for known devices" in out.stdout, out.stdout
    assert "no known device found" in out.stdout, out.stdout
    assert out.returncode != 0
    out = _run(app_exe, port_a, "bt.profile.remove ghost")
    assert "no profile 'ghost'" in out.stdout or out.returncode == 0


@pytest.mark.hardware
def test_raw_at_and_cert_list(app_exe, port_a):
    out = _run(app_exe, port_a, "at +GMM", "cert.list")
    assert out.returncode == 0, out.stdout
    assert "NORA-W36" in out.stdout


@pytest.mark.hardware
def test_wifi_regdomain_set_and_read_back(app_exe, port_a, tmp_path):
    out = subprocess.run(_args(app_exe, port_a, "wifi.regdomain fcc", "wifi.regdomain"), capture_output=True, text=True,
                         timeout=60, errors="replace", cwd=tmp_path)
    assert out.returncode == 0, out.stdout
    assert out.stdout.count("regdomain=fcc") == 2, out.stdout
    assert (tmp_path / "u-connectClientApp.regdomain").read_text() == "fcc"
    restore = subprocess.run(_args(app_exe, port_a, "wifi.regdomain etsi"), capture_output=True, text=True, timeout=60,
                             errors="replace", cwd=tmp_path)
    assert "regdomain=etsi" in restore.stdout, restore.stdout


@pytest.mark.hardware
def test_bt_settings_and_bonds(app_exe, port_a):
    out = _run(app_exe, port_a, "bt.defaults", "bt.pairing on", "bt.iocap none", "bt.security none",
               "bt.autoconfirm on", "bt.bonds", "bt.list")
    assert out.returncode == 0, out.stdout


@pytest.mark.hardware
def test_hid_and_gatt_server_definitions(app_exe, port_a):
    out = _run(app_exe, port_a, "hid.setup", "hid.status", "gatt.server.list")
    assert out.returncode == 0, out.stdout


@pytest.mark.hardware
def test_gatt_server_example_runs_and_stops(app_exe, port_a):
    out = _run(app_exe, port_a, "gatt.server.example.nus 3", "device.reboot")
    assert out.returncode == 0, out.stdout


@pytest.mark.hardware
def test_access_point_enable_status_disable(app_exe, port_a):
    out = _run(app_exe, port_a, "device.reboot")
    assert out.returncode == 0, out.stdout
    time.sleep(5)
    out = _run(app_exe, port_a, "wifi.ap.enable ucxtest-ap 12345678 6", "wifi.ap.status", "wifi.ap.disable",
               "device.reboot")
    assert out.returncode == 0, out.stdout
    assert "ucxtest-ap" in out.stdout, out.stdout
    assert "0 station(s)" in out.stdout, out.stdout


@pytest.mark.hardware
def test_network_commands_over_wifi(app_exe, port_a):
    ssid, password = os.environ.get("UCX_WIFI_SSID"), os.environ.get("UCX_WIFI_PASS", "")
    if not ssid:
        pytest.skip("set UCX_WIFI_SSID (and UCX_WIFI_PASS) to run the network test")
    out = _run(app_exe, port_a, f"wifi.connect {ssid} {password}", "wifi.status", "net.dns example.com",
               "net.ping 8.8.8.8 2", "http.get http://example.com 64", "wifi.disconnect", timeout=180)
    assert out.returncode == 0, out.stdout
    assert "wifi: connected" in out.stdout, out.stdout
    assert "200" in out.stdout, out.stdout


def _wifi_credentials():
    ssid, password = os.environ.get("UCX_WIFI_SSID"), os.environ.get("UCX_WIFI_PASS", "")
    if not ssid:
        pytest.skip("set UCX_WIFI_SSID (and UCX_WIFI_PASS) to run this test")
    return ssid, password


def _pc_lan_address():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]


@pytest.mark.hardware
def test_tcp_socket_round_trip_with_local_server(app_exe, port_a):
    ssid, password = _wifi_credentials()
    received = []
    server = socket.create_server(("", 0))
    server.settimeout(60)

    def serve():
        try:
            conn, _ = server.accept()
        except OSError:
            return
        with conn:
            conn.sendall(b"hello-from-pc")
            conn.settimeout(20)
            try:
                received.append(conn.recv(100))
            except OSError:
                pass

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    try:
        host, port = _pc_lan_address(), server.getsockname()[1]
        _run(app_exe, port_a, "device.reboot")
        time.sleep(5)
        out = _run(app_exe, port_a, f"wifi.connect {ssid} {password}", "sock.open tcp", f"sock.connect 0 {host} {port}",
                   "sock.list", "sock.read 0 64 10", "sock.send 0 ping-from-module", "sock.close 0", "sock.list",
                   timeout=180)
    finally:
        thread.join(timeout=30)
        server.close()
    if "timeout waiting for socket connection" in out.stdout:
        pytest.skip("module cannot reach this PC (firewall or Wi-Fi client isolation)")
    assert out.returncode == 0, out.stdout
    assert "socket 0: TCP, connected" in out.stdout, out.stdout
    assert "hello-from-pc" in out.stdout, out.stdout
    assert received == [b"ping-from-module"], (received, out.stdout)
    assert "0 socket(s)" in out.stdout, out.stdout


@pytest.mark.hardware
def test_udp_socket_opens_and_closes(app_exe, port_a):
    ssid, password = _wifi_credentials()
    _run(app_exe, port_a, "device.reboot")
    time.sleep(5)
    out = _run(app_exe, port_a, f"wifi.connect {ssid} {password}", "sock.open udp", "sock.list", "sock.close 0",
               timeout=120)
    assert out.returncode == 0, out.stdout
    assert "socket 0: UDP" in out.stdout, out.stdout


@pytest.mark.hardware
def test_mqtt_publish_is_received_back(app_exe, port_a):
    ssid, password = _wifi_credentials()
    topic = f"ucxapp/test/{int(time.time())}"
    out = _run(app_exe, port_a, f"wifi.connect {ssid} {password}", "mqtt.connect", f"mqtt.subscribe {topic} 0",
               f"mqtt.publish {topic} hello-mqtt", "mqtt.read 10", f"mqtt.unsubscribe {topic}", "mqtt.disconnect",
               timeout=180)
    if "connect failed" in out.stdout or "timeout waiting for the broker" in out.stdout:
        pytest.skip("public MQTT broker unreachable")
    assert out.returncode == 0, out.stdout
    assert f"{topic}: hello-mqtt" in out.stdout, out.stdout


@pytest.mark.hardware
def test_http_download_saves_a_file(app_exe, port_a, tmp_path):
    ssid, password = _wifi_credentials()
    target = tmp_path / "example.html"
    out = _run(app_exe, port_a, f"wifi.connect {ssid} {password}", f"http.download http://example.com/ {target}",
               timeout=180)
    assert out.returncode == 0, out.stdout
    assert "bytes saved" in out.stdout, out.stdout
    assert b"Example Domain" in target.read_bytes()


@pytest.mark.hardware
def test_certificate_upload_info_and_remove(app_exe, port_a, tmp_path):
    x509 = pytest.importorskip("cryptography.x509")
    import datetime

    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.x509.oid import NameOID

    key = ec.generate_private_key(ec.SECP256R1())
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "ucxapp test ca")])
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key())
            .serial_number(x509.random_serial_number()).not_valid_before(now - datetime.timedelta(days=1))
            .not_valid_after(now + datetime.timedelta(days=30))
            .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True).sign(key, hashes.SHA256()))
    pem = tmp_path / "ucxtest.pem"
    pem.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    fingerprint = cert.fingerprint(hashes.SHA256()).hex()

    _run(app_exe, port_a, "cert.remove ucxtest")  # leftover from an aborted run
    out = _run(app_exe, port_a, f"cert.upload ca {pem} ucxtest", "cert.list", "cert.info ucxtest",
               "cert.remove ucxtest", "cert.list")
    assert out.returncode == 0, out.stdout
    assert "uploaded ca ucxtest" in out.stdout, out.stdout
    assert "fingerprint verified" in out.stdout, out.stdout
    assert fingerprint in out.stdout.lower(), out.stdout
    assert "removed ca ucxtest" in out.stdout, out.stdout


@pytest.mark.hardware
def test_module_hosted_web_server(app_exe, port_a):
    ssid, password = os.environ.get("UCX_WIFI_SSID"), os.environ.get("UCX_WIFI_PASS", "")
    if not ssid:
        pytest.skip("set UCX_WIFI_SSID (and UCX_WIFI_PASS) to run the web server test")
    _run(app_exe, port_a, "device.reboot")  # a listener created after a Wi-Fi reconnect can reset connections
    time.sleep(5)
    args = _args(app_exe, port_a, f"wifi.connect {ssid} {password}", "web.start 80 abc123") + ["--shell"]
    proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            errors="replace")
    lines = queue.Queue()
    threading.Thread(target=lambda: [lines.put(line) for line in proc.stdout], daemon=True).start()
    try:
        url = None
        deadline = time.time() + 60
        while time.time() < deadline and url is None:
            try:
                match = re.search(r"http://[\d.]+:\d+/", lines.get(timeout=1))
            except queue.Empty:
                continue
            url = match.group(0) if match else None
        assert url, "web.start did not print a URL"

        page = urllib.request.urlopen(f"{url}?t=abc123", timeout=20)
        assert page.status == 200
        assert b"u-connectClientApp" in page.read()

        post = urllib.request.Request(f"{url}api/exec?t=abc123&cmd=device.info", method="POST")
        assert urllib.request.urlopen(post, timeout=20).status == 200

        for bad in (url, f"{url}?t=wrong"):
            with pytest.raises(urllib.error.HTTPError) as err:
                urllib.request.urlopen(bad, timeout=20)
            assert err.value.code == 403

        blocked = urllib.request.Request(f"{url}api/exec?t=abc123&cmd=web.stop", method="POST")
        with pytest.raises(urllib.error.HTTPError) as err:
            urllib.request.urlopen(blocked, timeout=20)
        assert err.value.code == 400
    finally:
        proc.stdin.write("web.stop\nexit\n")
        proc.stdin.flush()
        try:
            proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            proc.kill()


@pytest.mark.two_devices
def test_ble_module_b_discovers_module_a(app_exe, port_a, port_b):
    server = _start_server(app_exe, port_a)
    try:
        out = _run(app_exe, port_b, "gatt.listen 5", "bt.scan")
        assert DEVICE_NAME in out.stdout, out.stdout
    finally:
        server.terminate()
        server.wait(timeout=30)


@pytest.mark.pc_ble
def test_pc_ble_sees_advertising_module(app_exe, port_a):
    bleak = pytest.importorskip("bleak")
    server = _start_server(app_exe, port_a)

    async def scan():
        await asyncio.sleep(5)
        found = await bleak.BleakScanner.discover(timeout=10.0)
        return {d.name for d in found if d.name}, len(found)

    try:
        seen, total = asyncio.run(scan())
    except Exception as exc:  # no usable Bluetooth radio / adapter off
        pytest.skip(f"PC Bluetooth LE scan unavailable: {exc}")
    finally:
        server.terminate()
        server.wait(timeout=30)
    if total == 0:
        pytest.skip("PC Bluetooth LE radio saw no devices at all (adapter missing or off)")
    assert DEVICE_NAME in seen, f"{DEVICE_NAME} not among {sorted(seen)}"
