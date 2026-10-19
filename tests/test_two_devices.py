"""Client/server examples run against each other on two real NORA-W36 modules.

Module A plays the server/peripheral/access point, module B the client/central/station.
Run with:  set UCX_PORT_A=COM13 & set UCX_PORT_B=COM155 & pytest tests/test_two_devices.py
"""

import os
import re
import time

import pytest
from devices import (
    GATT_CLIENT_MENU,
    GATT_SERVER_MENU,
    SOCKET_MENU,
    SPS_MENU,
    WIFI_MENU,
    ble_link,
    connect,
    open_socket_menu,
    reboot,
    wifi_join,
    wifi_link,
)

pytestmark = pytest.mark.two_devices

GATT_PROFILES = {
    # name: (server key, client key, text B must show, text A must show when B subscribes)
    "battery": ("b", "b", r"\[BAS\] Battery Level: \d+ %", r"\[CCCD WRITE\].*Notifications"),
    # The notification URC does not reach the client app on this firmware pair, so only the
    # subscription (client CCCD write, seen by the server) is asserted.
    "heart_rate": ("h", "h", r"\[HRS\] Notifications ENABLED", r"\[CCCD WRITE\].*Notifications"),
    "environmental": ("e", "e", r"\[ESS\] Humidity notifications ENABLED", r"\[CCCD WRITE\].*Notifications"),
    "current_time": ("c", "c", r"\[CTS\] Read \d+ bytes", r"\[CCCD WRITE\].*Notifications"),
    "location_navigation": ("l", "l", r"\[LNS\] Read \d+ bytes", r"\[CCCD WRITE\].*Notifications"),
    "automation_io": ("a", "a", r"\[AIO\] Analog value: -?\d+", r"\[CCCD WRITE\].*Notifications"),
    # Device Information is built into the module, so nothing has to be activated on A.
    "device_information": (None, "d", r"\[DIS\] Manufacturer\s*: \S+", None),
}


@pytest.fixture
def pair(make_app, port_a, port_b):
    a = connect(make_app, "a", port_a)
    b = connect(make_app, "b", port_b)
    reboot(a)
    reboot(b)
    return a, b


@pytest.mark.parametrize("profile", GATT_PROFILES)
def test_gatt_server_profile_read_by_client(pair, profile):
    server_key, client_key, client_text, server_text = GATT_PROFILES[profile]
    a, b = pair

    if server_key:
        a.menu("e", GATT_SERVER_MENU)
        a.send(server_key)
        i, _ = a.expect_any([r"activated", r"ERROR"], 30)
        assert i == 0, f"{profile} service could not be activated"
        a.return_to_menu(GATT_SERVER_MENU, 15)
        a.back_to_main()

    ble_link(a, b)

    b.menu("g", GATT_CLIENT_MENU)
    b.send(client_key)
    prompt = r"Enter service index to discover"
    while b.expect_any([client_text, prompt], 40)[0] == 1:
        b.send("")
    if server_text:
        a.expect(server_text, 20)


def test_nus_client_subscribes_and_sends(pair):
    a, b = pair

    a.menu("e", GATT_SERVER_MENU)
    a.send("u")
    i, _ = a.expect_any([r"activated", r"ERROR"], 30)
    assert i == 0, "NUS service could not be activated"
    a.expect(r"Press any key when client has connected", 15)
    a.send("")
    a.return_to_menu(GATT_SERVER_MENU, 15)
    a.back_to_main()

    ble_link(a, b)

    b.menu("g", GATT_CLIENT_MENU)
    b.send("u")
    prompt = r"Enter service index to discover"
    while b.expect_any([r"Enter text to send", prompt], 40)[0] == 1:
        b.send("")
    # The write-without-response URC does not reach the server app on this firmware pair.
    a.expect(r"\[UART\] Client enabled TX notifications", 20)
    b.send("hello-nus")
    b.expect(r"\[UART TX\] Sending: hello-nus", 10)
    b.expect(r"Enter text to send", 10)


def test_sps_data_both_directions(pair):
    a, b = pair

    a.menu("s", SPS_MENU)
    a.send("1")
    a.expect(r"SPS service enabled", 15)
    a.return_to_menu(SPS_MENU, 15)
    a.back_to_main()

    ble_link(a, b)

    b.menu("s", SPS_MENU)
    b.send("2")
    b.expect(r"SPS Connection established", 20)
    b.send("3")
    b.expect(r"Enter data to send:", 10)
    b.send("hello-from-b")
    a.expect(r"\[SPS RX\] conn=\d+, \d+ bytes: hello-from-b", 20)

    a.menu("s", SPS_MENU)
    a.send("3")
    a.expect(r"Enter data to send:", 10)
    a.send("hello-from-a")
    b.expect(r"\[SPS RX\] conn=\d+, \d+ bytes: hello-from-a", 20)


def _connect_socket(b, host, port, wait_connected=False):
    b.send("3")
    b.expect(r"Enter hostname/IP", 10)
    b.send(host)
    b.expect(r"Enter port", 10)
    b.send(str(port))
    i, _ = b.expect_any([r"Use TLS/SSL", r"Successfully connected", r"Failed to connect"], 20)
    if i == 0:
        b.send("n")
        i, _ = b.expect_any([r"Successfully connected", r"Failed to connect"], 20)
        i += 1
    assert i == 1, f"socket connect to {host}:{port} failed"
    if wait_connected:
        # A TCP socket accepts data only after the connected URC, which can trail the menu.
        b.expect(r"Socket connected: \d+", 20)
    else:
        b.return_to_menu(SOCKET_MENU, 15)


def _tcp_server(a, port):
    open_socket_menu(a)
    a.send("1")
    a.return_to_menu(SOCKET_MENU, 15)
    a.send("5")
    a.expect(r"Enter local port", 10)
    a.send(str(port))
    a.expect(r"bound to local port", 10)
    a.return_to_menu(SOCKET_MENU, 15)
    a.send("6")
    a.expect(r"now listening", 10)
    a.return_to_menu(SOCKET_MENU, 15)


def _tcp_client_send(b, host, port, text):
    open_socket_menu(b)
    b.send("1")
    b.return_to_menu(SOCKET_MENU, 15)
    _connect_socket(b, host, port, wait_connected=True)
    b.send("4")
    b.expect(r"Enter data to send:", 10)
    b.send(text)


def test_tcp_client_sends_to_server_over_ap(pair):
    a, b = pair
    gateway = wifi_link(a, b)

    _tcp_server(a, 5000)
    _tcp_client_send(b, gateway, 5000, "hello-tcp")

    a.expect(r"\[SOCKET RX\] socket=\d+, \d+ bytes: hello-tcp", 20)


@pytest.fixture
def home_wifi():
    ssid, password = os.environ.get("UCX_WIFI_SSID"), os.environ.get("UCX_WIFI_PASSWORD")
    if not ssid or password is None:
        pytest.skip("set UCX_WIFI_SSID and UCX_WIFI_PASSWORD to run the infrastructure Wi-Fi tests")
    return ssid, password


def test_tcp_between_modules_on_infrastructure_wifi(pair, home_wifi):
    a, b = pair
    ip_a, _ = wifi_join(a, *home_wifi)
    wifi_join(b, *home_wifi)

    _tcp_server(a, 5002)
    _tcp_client_send(b, ip_a, 5002, "hello-lan")

    a.expect(r"\[SOCKET RX\] socket=\d+, \d+ bytes: hello-lan", 20)


def test_wifi_scan_sees_infrastructure_network(pair, home_wifi):
    a, _ = pair
    a.menu("w", WIFI_MENU)
    a.send("3")
    a.expect(re.escape(home_wifi[0]), 40)


def test_udp_client_sends_to_server_over_ap(pair):
    a, b = pair
    gateway = wifi_link(a, b)

    open_socket_menu(a)
    a.send("2")
    a.return_to_menu(SOCKET_MENU, 15)
    a.send("5")
    a.expect(r"Enter local port", 10)
    a.send("5001")
    a.expect(r"bound to local port", 10)
    a.return_to_menu(SOCKET_MENU, 15)

    open_socket_menu(b)
    b.send("2")
    b.return_to_menu(SOCKET_MENU, 15)
    _connect_socket(b, gateway, 5001)
    b.send("4")
    b.expect(r"Enter data to send:", 10)
    b.send("hello-udp")

    a.expect(r"\[SOCKET RX\] socket=\d+, \d+ bytes: hello-udp", 20)


MQTT_MENU = r"MQTT EXAMPLES"
HTTP_MENU = r"HTTP CLIENT & REST API"


def _mqtt_connect(app):
    # The public broker occasionally drops a fresh session right away, so retry.
    app.menu("m", MQTT_MENU)
    for _ in range(3):
        app.send("1")
        app.expect(r"Use MQTTS", 10)
        app.send("n")
        i, _m = app.expect_any([r"Connected to MQTT broker successfully", r"Failed to connect"], 60)
        assert i == 0, "could not connect to the MQTT broker"
        # The session is only usable once the connected URC has arrived (printed after the menu).
        app.return_to_menu(MQTT_MENU, 15)
        i, _m = app.expect_any([r"MQTT CONNECTION ESTABLISHED", r"MQTT DISCONNECTED"], 30)
        if i == 0:
            return
    raise AssertionError("MQTT broker kept dropping the session")


def test_mqtt_publish_reaches_subscriber(pair, home_wifi):
    a, b = pair
    wifi_join(a, *home_wifi)
    wifi_join(b, *home_wifi)
    topic = f"ucxtest/{os.getpid()}/{int(time.time())}"

    _mqtt_connect(b)
    b.send("3")
    b.expect(r"Enter topic to subscribe", 10)
    b.send(topic)
    b.expect(r"QoS:", 10)
    b.send("0")
    b.expect(r"Subscribed successfully", 15)
    b.return_to_menu(MQTT_MENU, 15)

    _mqtt_connect(a)
    a.send("5")
    a.expect(r"Enter topic:", 10)
    a.send(topic)
    a.expect(r"Enter message:", 10)
    a.send("hello-mqtt")
    a.expect(r"QoS:", 10)
    a.send("0")
    a.expect(r"Retain message", 10)
    a.send("n")
    a.expect(r"published successfully", 15)

    b.expect(r"\[MQTT RX\] topic=\S*" + re.escape(topic) + r", \d+ bytes: hello-mqtt", 30)


def test_http_get_over_infrastructure_wifi(pair, home_wifi):
    a, _ = pair
    wifi_join(a, *home_wifi)

    a.menu("h", HTTP_MENU)
    a.send("1")
    a.expect(r"URL or Host", 20)
    a.send("http://httpbin.org/get")
    a.expect(r"Save response to file", 10)
    a.send("")
    a.expect(r"Add custom HTTP headers", 10)
    a.send("n")
    a.expect(r"Response received: [1-9]\d* bytes total", 60)
