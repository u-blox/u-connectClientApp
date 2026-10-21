"""Tests against real NORA-W36 modules (a second module is the peer; the PC is never a counterpart).

The tests adapt to what is attached (see conftest.py); `ucxapp --detect --test tests/test_hardware.py` finds it.
"""

import os
import queue
import re
import subprocess
import threading
import time

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


@pytest.mark.app
def test_imu_detect_probes_usb_uart_adapters(app_exe):
    # "USB-SERIAL CH340" was once parsed as an EVK port with a label and silently skipped.
    ports = subprocess.run([str(app_exe), "--list-ports"], capture_output=True, text=True, timeout=30,
                           errors="replace").stdout
    adapters = re.findall(r"^(COM\d+)\s+available\s+.*USB-SERIAL", ports, re.MULTILINE)
    if not adapters:
        pytest.skip("no USB-SERIAL adapter attached")
    out = _run(app_exe, "COM255", "imu.detect", timeout=300)
    for port in adapters:
        assert f"  {port} (" in out.stdout, out.stdout


@pytest.mark.hardware
def test_wifi_scan_finds_networks(app_exe, port_a):
    out = _run(app_exe, port_a, "wifi.scan")
    assert out.returncode == 0, out.stdout


@pytest.mark.hardware
def test_wifi_profile_auto_falls_back_to_scan(app_exe, port_a, tmp_path):
    ssid, password = os.environ.get("UCX_WIFI_SSID"), os.environ.get("UCX_WIFI_PASS", "")
    if not ssid:
        pytest.skip("set UCX_WIFI_SSID (and UCX_WIFI_PASS) to run the Wi-Fi profile test")
    (tmp_path / "ucxapp.wifilast").write_text("bogus")
    out = subprocess.run(
        _args(app_exe, port_a, "wifi.profile.add bogus no-such-network-xyz pw12345678",
              f"wifi.profile.add real {ssid} {password}", "wifi.profile.auto", "wifi.status"),
        capture_output=True, text=True, timeout=180, errors="replace", cwd=tmp_path)
    assert out.returncode == 0, out.stdout
    assert "Strongest known network: 'real'" in out.stdout, out.stdout
    assert (tmp_path / "ucxapp.wifilast").read_text() == "real"


@pytest.mark.hardware
def test_ble_scan_completes(app_exe, port_a):
    out = _run(app_exe, port_a, "bt.scan")
    assert out.returncode == 0, out.stdout


@pytest.mark.hardware
def test_ble_profile_auto_falls_back_to_scan(app_exe, port_a, tmp_path):
    (tmp_path / "ucxapp.blelast").write_text("ghost")
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
    assert (tmp_path / "ucxapp.regdomain").read_text() == "fcc"
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


def _wifi_connect_command():
    """UCX_WIFI_SSID/_PASS when set, else the strongest saved profile (wifi.profile.auto) from the repo ini."""
    ssid = os.environ.get("UCX_WIFI_SSID")
    return f"wifi.connect {ssid} {os.environ.get('UCX_WIFI_PASS', '')}" if ssid else "wifi.profile.auto"


def _wifi_credentials():
    ssid, password = os.environ.get("UCX_WIFI_SSID"), os.environ.get("UCX_WIFI_PASS", "")
    if not ssid:
        pytest.skip("set UCX_WIFI_SSID (and UCX_WIFI_PASS) to run this test")
    return ssid, password


PING_RE = re.compile(r"sent (\d+), received (\d+), average (\d+) ms")
PING_MAX_AVG_MS = 1000


def _join_wifi(exe, port, ssid, password):
    """Joins the Wi-Fi and returns (module ip, gateway) from the link details."""
    out = _run(exe, port, f"wifi.connect {ssid} {password}", timeout=180)
    assert out.returncode == 0, out.stdout
    ip, gateway = re.search(r"ip=(\S+)", out.stdout), re.search(r"gateway (\S+)", out.stdout)
    assert ip and gateway, out.stdout
    return ip.group(1), gateway.group(1)


def _assert_ping(out, target):
    match = PING_RE.search(out.stdout)
    assert match, f"ping {target}: {out.stdout}"
    sent, received, avg = map(int, match.groups())
    assert received == sent, f"ping {target} lost packets: {out.stdout}"
    assert avg < PING_MAX_AVG_MS, f"ping {target} too slow: {out.stdout}"


@pytest.fixture(scope="module")
def wifi_peers(app_exe, port_a, port_b):
    """Both modules joined to the Wi-Fi: {port: (ip, gateway)}."""
    ssid, password = _wifi_credentials()
    return {port: _join_wifi(app_exe, port, ssid, password) for port in (port_a, port_b)}


@pytest.mark.two_devices
def test_wifi_ping_gateway_from_both_modules(app_exe, wifi_peers, port_a, port_b):
    for port in (port_a, port_b):
        _, gateway = wifi_peers[port]
        _assert_ping(_run(app_exe, port, f"net.ping {gateway} 4"), gateway)


@pytest.mark.two_devices
def test_wifi_ping_between_modules_both_directions(app_exe, wifi_peers, port_a, port_b):
    ip_a, ip_b = wifi_peers[port_a][0], wifi_peers[port_b][0]
    for source, target in ((port_a, ip_b), (port_b, ip_a)):
        _assert_ping(_run(app_exe, source, f"net.ping {target} 4"), target)


@pytest.mark.two_devices
def test_tcp_socket_between_two_modules(app_exe, port_a, port_b):
    """Module A hosts the web server (a TCP listener); module B opens a raw TCP socket to it."""
    ssid, password = _wifi_credentials()
    for port in (port_a, port_b):
        _run(app_exe, port, "device.reboot")
    time.sleep(5)
    args = _args(app_exe, port_a, f"wifi.connect {ssid} {password}", "web.start 80 abc123") + ["--shell"]
    server = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                              errors="replace")
    lines = queue.Queue()
    threading.Thread(target=lambda: [lines.put(line) for line in server.stdout], daemon=True).start()
    try:
        host = None
        deadline = time.time() + 60
        while time.time() < deadline and host is None:
            try:
                match = re.search(r"http://([\d.]+):80/", lines.get(timeout=1))
            except queue.Empty:
                continue
            host = match.group(1) if match else None
        assert host, "module A did not print its web URL"
        request = "sock.send 0 {} HTTP/1.0\\r\\n\\r\\n"
        out = _run(app_exe, port_b, f"wifi.connect {ssid} {password}",
                   "sock.open tcp", f"sock.connect 0 {host} 80", request.format("GET /?t=abc123"), "sock.read 0 120 10",
                   "sock.close 0",
                   "sock.open tcp", f"sock.connect 0 {host} 80", request.format("GET /?t=wrong"), "sock.read 0 120 10",
                   "sock.close 0", timeout=240)
    finally:
        server.stdin.write("web.stop\nexit\n")
        server.stdin.flush()
        try:
            server.wait(timeout=30)
        except subprocess.TimeoutExpired:
            server.kill()
    assert out.returncode == 0, out.stdout
    assert "200" in out.stdout, out.stdout   # valid token
    assert "403" in out.stdout, out.stdout   # wrong token


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
    assert "no subscribe confirmation" not in out.stdout, out.stdout
    assert "read failed" not in out.stdout, out.stdout
    assert "publish failed" not in out.stdout, out.stdout


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


@pytest.mark.two_devices
def test_ble_module_b_discovers_module_a(app_exe, port_a, port_b):
    server = _start_server(app_exe, port_a)
    try:
        out = _run(app_exe, port_b, "gatt.listen 5", "bt.scan")
        assert DEVICE_NAME in out.stdout, out.stdout
    finally:
        server.terminate()
        server.wait(timeout=30)


@pytest.mark.gnss
def test_gnss_reads_live_data(app_exe, gnss):
    port, baud = gnss
    proc = subprocess.Popen([str(app_exe), "COM255"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            text=True, errors="replace")
    try:
        for command, wait in ((f"gnss.open {port} {baud}", 4), ("gnss.status", 1), ("gnss.sats", 1),
                              ("gnss.close", 1), ("exit", 0)):
            proc.stdin.write(command + "\n")
            proc.stdin.flush()
            time.sleep(wait)
        out, _ = proc.communicate(timeout=10)
    finally:
        proc.kill()
    assert "NMEA" in out and "0 NMEA" not in out, out
    assert "Fix:" in out and "satellite(s)" in out


def _close_listeners(exe, port):
    """Close TCP listeners left on the module by an earlier run, they keep the bridge port bound."""
    handles = re.findall(r"socket (\d+): TCP, listening", _run(exe, port, "sock.list").stdout)
    if handles:
        _run(exe, port, *[f"sock.close {handle}" for handle in handles])


def _start_bridge(exe, port, source, found, setup, bridge):
    """Module A runs the source bridge in a shell; returns (process, line queue)."""
    _close_listeners(exe, port)
    args = _args(exe, port, *setup, f"{source}.open {found[0]} {found[1]}".strip(), bridge) + ["--shell"]
    proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            errors="replace")
    lines = queue.Queue()
    threading.Thread(target=lambda: [lines.put(line) for line in proc.stdout], daemon=True).start()
    seen = []
    deadline = time.time() + 90
    while time.time() < deadline:
        try:
            line = lines.get(timeout=1)
        except queue.Empty:
            continue
        seen.append(line)
        if "bridge:" in line:
            return proc, seen
        if line.startswith("ERROR:"):
            _stop(proc)
            pytest.fail(f"{source} bridge failed to start: {''.join(seen)[-400:]}")
        if f"no {source.upper()} found" in line or "no receiver" in line or "no IMU" in line:
            break
    _stop(proc)
    pytest.skip(f"{source} not available for the bridge: {''.join(seen)[-400:]}")


def _stop(proc):
    try:
        proc.stdin.write("exit\n")
        proc.stdin.flush()
        proc.wait(timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        proc.kill()


def _check_stream(source, text):
    if source == "gnss":
        assert "$G" in text, text
    else:
        assert re.search(r"received [1-9]\d* bytes|\[NUS RX\]", text), text


SOURCES = [pytest.param("gnss", marks=pytest.mark.gnss), pytest.param("imu", marks=pytest.mark.imu)]


@pytest.mark.two_devices
@pytest.mark.parametrize("source", SOURCES)
def test_bridge_over_wifi_between_two_modules(app_exe, port_a, port_b, source, request):
    """A bridges the source over TCP on Wi-Fi; B connects a socket and reads the stream."""
    wifi = _wifi_connect_command()
    proc, seen = _start_bridge(app_exe, port_a, source, request.getfixturevalue(source), [wifi],
                               f"{source}.bridge tcp 2947")
    try:
        match = re.search(r"connect a TCP client to ([\d.]+):2947", "".join(seen))
        assert match, "".join(seen)
        out = _run(app_exe, port_b, wifi, "sock.open tcp", f"sock.connect 0 {match.group(1)} 2947",
                   "sock.read 0 400 10", "sock.close 0", timeout=240)
    finally:
        _stop(proc)
    assert out.returncode == 0, out.stdout
    _check_stream(source, out.stdout)


@pytest.mark.two_devices
@pytest.mark.parametrize("source", SOURCES)
def test_bridge_over_ble_between_two_modules(app_exe, port_a, port_b, source, request):
    """A bridges the source over Nordic UART; B scans, connects by name and listens for notifications."""
    name = "ucxbridge"
    proc, _ = _start_bridge(app_exe, port_a, source, request.getfixturevalue(source), [f'at +UBTLN="{name}"'],
                            f"{source}.bridge ble nus")
    try:
        out = _run(app_exe, port_b, "bt.scan", f"bt.connect {name}", "gatt.client.example.nus", "gatt.listen 15",
                   "bt.disconnect", timeout=240)
    finally:
        _stop(proc)
    assert out.returncode == 0, out.stdout
    assert "[NUS RX]" in out.stdout, out.stdout
    if source == "gnss":
        assert "$G" in out.stdout, out.stdout
