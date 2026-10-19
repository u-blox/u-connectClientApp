"""Tests against real NORA-W36 modules and the PC's built-in Bluetooth LE radio.

Run with e.g.:  set UCX_PORT_A=COM5 & set UCX_PORT_B=COM7 & pytest tests/test_hardware.py
"""

import asyncio

import pytest
from devices import BT_MENU, WIFI_MENU
from devices import connect as _connect
from devices import enable_advertising as _enable_advertising


@pytest.mark.hardware
def test_connects_and_reports_model(make_app, port_a):
    app = make_app("a", port=port_a)
    assert app.wait_connected(60).startswith("NORA-W36")
    assert app.quit()


@pytest.mark.hardware
def test_wifi_scan_finds_networks(make_app, port_a):
    app = _connect(make_app, "a", port_a)
    app.menu("w", WIFI_MENU)
    app.send("3")
    app.expect(r"Found \d+ network", 90)
    app.return_to_menu(WIFI_MENU, 15)
    app.back_to_main()
    assert app.quit()


@pytest.mark.hardware
def test_ble_scan_completes(make_app, port_a):
    app = _connect(make_app, "a", port_a)
    app.menu("b", BT_MENU)
    app.send("2")
    app.expect(r"Found \d+ unique device|No devices found", 60)
    app.return_to_menu(BT_MENU, 15)
    app.back_to_main()
    assert app.quit()


@pytest.mark.two_devices
def test_ble_module_b_discovers_module_a(make_app, port_a, port_b):
    a = _connect(make_app, "a", port_a)
    b = _connect(make_app, "b", port_b)
    _enable_advertising(a)

    b.menu("b", BT_MENU)
    b.send("2")
    b.expect(r"Found \d+ unique device|No devices found", 60)
    assert f"Name: {a.ble_name}" in b.output


@pytest.mark.pc_ble
def test_pc_ble_sees_advertising_module(make_app, port_a):
    bleak = pytest.importorskip("bleak")
    a = _connect(make_app, "a", port_a)
    _enable_advertising(a)

    async def scan():
        found = await bleak.BleakScanner.discover(timeout=10.0)
        return {d.name for d in found if d.name}, len(found)

    try:
        seen, total = asyncio.run(scan())
    except Exception as exc:  # no usable Bluetooth radio / adapter off
        pytest.skip(f"PC Bluetooth LE scan unavailable: {exc}")
    if total == 0:
        pytest.skip("PC Bluetooth LE radio saw no devices at all (adapter missing or off)")
    assert a.ble_name in seen, f"{a.ble_name} not among {sorted(seen)}"
