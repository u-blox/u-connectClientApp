"""Menu-driving helpers shared by the hardware tests (app text patterns follow ucx-windows-app.c)."""

import re

BT_MENU = r"BLUETOOTH MENU"
WIFI_MENU = r"WI-FI STATION MENU"
WIFI_AP_MENU = r"WI-FI ACCESS POINT MENU"
WIFI_FUNCTIONS_MENU = r"WI-FI FUNCTIONS"
SOCKET_MENU = r"SOCKET MENU"
SPS_MENU = r"SERIAL PORT SERVICE"
GATT_SERVER_MENU = r"GATT SERVER \(Provides"
GATT_CLIENT_MENU = r"GATT CLIENT \(Connect"
MAC = r"[0-9A-F]{2}(?::[0-9A-F]{2}){5}"


def connect(make_app, name, port):
    """Start the app on `port`, wait for the main menu; remembers the module's BLE name as `ble_name`."""
    app = make_app(name, port=port)
    app.wait_connected(60)
    app.ble_name = app.expect(r"Bluetooth:\s+(\S+)", 10).group(1)
    app.expect(r"Choice:", 10)
    return app


def reboot(app):
    """Reboot the module so GATT services, AP and sockets from earlier runs are gone."""
    app.send("r")
    app.expect(r"Module has rebooted successfully", 40)
    app.expect(r"Choice:", 30)


def enable_advertising(app):
    app.menu("b", BT_MENU)
    app.menu("6", r"Choice:")
    app.send("1")
    app.expect(r"advertising ENABLED", 15)
    app.return_to_menu(BT_MENU, 15)


def ble_link(a, b):
    """Make `a` advertise and `b` connect to it; both apps end at their main menu."""
    enable_advertising(a)
    a.back_to_main()

    b.menu("b", BT_MENU)
    b.send("2")
    addr = b.expect(rf"Address: ({MAC})\s+Name: {re.escape(a.ble_name)}\b", 60).group(1)
    b.return_to_menu(BT_MENU, 40)

    b.send("3")
    b.expect(r"Choice:", 10)
    b.send("m")
    b.expect(r"Enter Bluetooth address", 10)
    b.send(addr)
    i, _ = b.expect_any([r"Connected successfully", r"Failed to connect"], 20)
    assert i == 0, f"BLE connect to {addr} failed"
    b.return_to_menu(BT_MENU, 15)
    b.back_to_main()


def wifi_link(a, b, ssid="ucxtest-ap", password="ucxtest123"):
    """Run an access point on `a` and join it with `b`; returns the AP's IP (b's gateway)."""
    a.menu("o", WIFI_AP_MENU)
    a.menu("1", r"Enter AP SSID")
    a.send(ssid)
    a.expect(r"Enter WPA2 password", 10)
    a.send(password)
    i, _ = a.expect_any([r"Access Point ENABLED", r"ERROR"], 30)
    assert i == 0, "AP activation failed"
    a.return_to_menu(WIFI_AP_MENU, 30)
    a.back_to_main()

    b.menu("w", WIFI_MENU)
    b.menu("4", r"Enter SSID:")
    b.send(ssid)
    b.expect(r"Enter password", 10)
    b.send(password)
    i, m = b.expect_any([r"Gateway: (\d+\.\d+\.\d+\.\d+)", r"Connection failed|ERROR"], 40)
    assert i == 0, "station could not join the AP"
    gateway = m.group(1)
    b.return_to_menu(WIFI_MENU, 90)
    b.back_to_main()
    return gateway


def open_socket_menu(app):
    app.menu("n", WIFI_FUNCTIONS_MENU)
    app.menu("1", SOCKET_MENU)
