# u-connectClientApp

**Talk to your u-connectXpress module in one line.** Scan Wi-Fi, join a network, open sockets, publish over MQTT, fetch HTTPS pages, pair Bluetooth devices and update firmware, from an interactive shell, a script or a web page. The same commands run on Windows, Linux and on an STM32 board.

Supported modules: **NORA-W36** (Wi-Fi + Bluetooth LE) and **NORA-B26** (Bluetooth LE).

| Windows | Linux | STM32 firmware |
|---|---|---|
| Signed `.exe`, FTDI driver built in | Single binary | NUCLEO-H753ZI, NUCLEO-F767ZI, NUCLEO-F439ZI |

Download ready-made builds from the [Releases page](https://github.com/u-blox/u-connectClientApp/releases). No installation needed.

## Try it in 30 seconds

```powershell
ucxapp COM13 --exec "device.info"
```

Leave out the port and the app finds your EVK by itself. Type `help` in the shell to list every command.

## Powerful one-liners

**Get online and fetch a web page**
```powershell
ucxapp COM13 --exec "wifi.connect MySsid MyPassword" --exec "net.ping u-blox.com" --exec "http.get https://example.com"
```

**Scan the air for networks**
```powershell
ucxapp COM13 --exec "wifi.scan"
```

**Update the module firmware over XMODEM, with SHA256 verification**
```powershell
ucxapp COM13 --exec "fw.flash module.bin"
```

**Always the newest firmware**: look up the latest GitHub release for the connected module and flash it
```powershell
ucxapp COM13 --exec "fw.download latest module.zip" --exec "fw.flash module.zip"
```

**Download firmware straight from GitHub and flash it**: the module fetches the release asset over Wi-Fi (redirects followed), the app verifies the SHA256 and flashes it
```powershell
ucxapp COM13 --exec "wifi.connect MySsid MyPassword" --exec "fw.download https://github.com/<owner>/<repo>/releases/download/<tag>/module.bin module.bin <sha256>" --exec "fw.flash module.bin"
```
`fw.download` deletes the file if the SHA256 does not match, so a bad download is never flashed. Without Wi-Fi, download the file in a browser and use `fw.flash` directly.

**Remote control from any browser**: the module serves its own web UI over Wi-Fi
```powershell
ucxapp COM13 --exec "wifi.connect MySsid MyPassword" --exec "web.start" --shell
```
It prints `http://<module-ip>:<port>/?t=<token>`. Open it on your phone or laptop for a command box with live output.

**Use it in CI and test scripts**: every `--exec` runs in order and the app exits with code 1 on the first failure.

## Highlights

- **Bluetooth**: scan, connect, bond, SPS, GATT client and server, saved profiles
- **Wi-Fi** (NORA-W36): station and AP modes, scanning, auto-reconnect, saved profiles
- **Network services**: sockets, HTTP/HTTPS, MQTT, TLS and certificates, ping, DNS
- **Firmware updates**: XMODEM with SHA256 verification
- **Scriptable**: `--exec` for one-shots, `--shell` for interactive use
- **Portable C11 command layer**: add a command by adding one table entry
- **On a microcontroller too**: run the same shell on an STM32 Nucleo board with the module wired to a UART, with a transparent `bridge` mode for AT debugging

## Commands at a glance

Type `help` in the shell, or `help wifi` to filter by prefix. Full list with arguments: [command reference](COMMANDS.md).

| Area | Commands |
|---|---|
| Device | `device.info`, `device.reboot`, `device.factory confirm`, `at <raw AT command>` |
| Wi-Fi | `wifi.scan`, `wifi.connect <ssid> [pw]`, `wifi.status`, `wifi.disconnect`, `wifi.ip`, `wifi.rssi`, `wifi.profile.*`, `wifi.ap.*` |
| Network | `net.ping <host>`, `net.dns <host>`, `http.get <url>`, `http.download <url> <file>` |
| Sockets | `sock.open`, `sock.connect`, `sock.send`, `sock.read`, `sock.listen`, `sock.accept`, `sock.sendto`, `sock.recvfrom`, `sock.close` |
| MQTT | `mqtt.connect`, `mqtt.subscribe`, `mqtt.publish`, `mqtt.read`, `mqtt.disconnect` |
| Bluetooth | `bt.scan`, `bt.connect`, `bt.pairing`, `bt.bonds`, `bt.profile.*`, `bt.list` |
| GATT | `gatt.services`, `gatt.read`, `gatt.write`, `gatt.subscribe`, `gatt.client.example.*`, `gatt.server.example.*` (heart rate, battery, NUS, SPS, Wi-Fi provisioning and more) |
| HID | `hid.setup`, `hid.text`, `hid.key` (act as a Bluetooth keyboard) |
| Utilities | `utils.qr.text`, `utils.qr.wifi`, `utils.qr.geo`, `utils.zip`, `utils.unzip` |
| Sensors | `gnss.detect`, `gnss.open`, `gnss.status`, `gnss.sats`, `gnss.bridge`, `imu.detect`, `imu.open`, `imu.status`, `imu.bridge` (PC serial sensors, bridged over TCP, UDP or BLE) |
| Certificates | `cert.list`, `cert.upload`, `cert.info`, `cert.remove` |
| Firmware | `fw.download [latest [file]]` or `fw.download <url> <file> [sha256]`, `fw.flash <file> [baud]` |
| Web UI | `web.start`, `web.stop`, `web.status` |

## Quick start from source

```powershell
git clone https://github.com/u-blox/u-connectClientApp.git
cd u-connectClientApp
git submodule update --init u-connectClient
.\ucxapp             # builds on first run, then starts the shell
```

Linux: `./ucxapp.py`. STM32: `.\ucxapp --stm32 --build --flash --log`.
Find attached modules, GNSS receivers and IMUs: `.\ucxapp --detect`.

## Documentation

- [Command reference](COMMANDS.md): every command with usage
- [Building, flashing and releasing](BUILDING.md): prerequisites, launchers, STM32 boards, signing, file structure
- [u-connectClient](https://github.com/u-blox/u-connectClient): the small-footprint AT client library underneath

---

# Disclaimer
Copyright &#x00a9; u-blox

u-blox reserves all rights in this deliverable (documentation, software, etc.,
hereafter “Deliverable”).

u-blox grants you the right to use, copy, modify and distribute the
Deliverable provided hereunder for any purpose without fee.

THIS DELIVERABLE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
WARRANTY. IN PARTICULAR, NEITHER THE AUTHOR NOR U-BLOX MAKES ANY
REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY OF THIS
DELIVERABLE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.

In case you provide us a feedback or make a contribution in the form of a
further development of the Deliverable (“Contribution”), u-blox will have the
same rights as granted to you, namely to use, copy, modify and distribute the
Contribution provided to us for any purpose without fee.
