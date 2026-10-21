# Command Layer

A small, portable layer for driving a u-connectXpress module with text commands instead of the
menu. It lives in `app/src/ucxapp_*` and has no Windows dependencies, so the same
commands can run in a PC console, a Linux terminal or a UART shell on an embedded target.

## Using it

```powershell
# One-shot: run commands in order, stop at the first failure (exit code 1)
ucxapp COM13 --exec "device.info" --exec "wifi.scan"

# Interactive shell
ucxapp COM13 --shell

# A single command can be given directly (no spaces, no quotes), port optional
ucxapp COM13 device.info
ucxapp device.info
```

- Add `--tui` for a three-pane text UI (see [Text UI](#text-ui)).

- Without a port argument, `--exec` and `--shell` use the saved port from the ini file.
- If the module cannot be connected, commands that need it fail with `not connected`; `help` still works.
- Arguments are whitespace separated; use double quotes for values with spaces
  (`wifi.connect "My Network" "my password"`). `\"` and `\\` escape inside quotes.
- Lines starting with `#` are comments. Maximum 8 arguments per command.

## Command reference

Commands are grouped like the u-connectXpress AT manual: general, system, Bluetooth, GATT, Wi-Fi,
network, sockets, MQTT, HTTP, security, then the app's own tools. `help <prefix>` lists a group
(`help wifi`, `help gatt.server`).

### General

| Command | Arguments | Description |
|---|---|---|
| `help` | `[prefix]` | List commands, optionally filtered (`help wifi`) |
| `at` | `[command]` | Send a raw AT command; the `AT` prefix is optional (`at +GMI`). With no command it enters AT mode: every line is sent as typed (quotes kept) until you type `back` |

### System

| Command | Arguments | Description |
|---|---|---|
| `device.info` | | Manufacturer, model, firmware version |
| `device.status` | | Overview: module info, Wi-Fi state and IP, open sockets, BLE connections, certificates |
| `device.reboot` | | Reboot the module (clears GATT services and connections); use as the last command |
| `device.factory` | `<confirm>` | Reset the module to factory settings (erases Wi-Fi, Bluetooth and certificate settings) and reboot; the word `confirm` is required |
| `device.connect` | `[port]` | Connect to the module (default: saved port). Windows only |
| `device.disconnect` | | Disconnect. Windows only |
| `fw.download` | `[latest [file]]` or `<url> <file> [sha256]` | With no arguments (same as `latest`) looks up the newest u-connectXpress GitHub release for the connected module family (e.g. NORA-W36) and saves its zip (Windows only); flash it with `fw.flash`. With a URL: download via the module (follows redirects), print SHA-256, delete the file on mismatch |
| `fw.bootloader` | `<port> <file.bin\|file.zip> [baud]` | Flash a module that is in bootloader mode (no AT interface): waits for the `>` prompt, sends `x`, then XMODEM; run `device.disconnect` first |
| `fw.flash` | `<file.bin\|file.zip> [baud]` | XMODEM firmware update (default 921600 baud); a zip is only extracted (needs `tar` on Windows, `unzip` elsewhere), and only the `.bin` inside is flashed |

```powershell
ucxapp COM13 --exec "device.info"
ucxapp COM13 --exec "fw.download latest" --exec "fw.flash NORA-W36X-3.1.0.zip"
ucxapp COM13 --exec "at +GMR"
```

### Bluetooth

| Command | Arguments | Description |
|---|---|---|
| `bt.scan` | `[full]` | Scan for BLE devices (up to 30 s). One line per device (duplicates merged, named first, strongest first) with what it advertises: services, manufacturer, iBeacon/Eddystone, appearance. `full` decodes the whole advertising data. The results are kept for `bt.connect` |
| `bt.connect` | `[addr[,random]\|n\|name]` | With no argument: connect to the last used profile, else scan and take the strongest known device (same as `bt.profile.auto`). Otherwise connect by address, by the number from the last `bt.scan`, or by profile or scanned device name (a unique start of the name is enough). Suggests `bt.profile.add` for devices not yet saved |
| `bt.disconnect` | `<handle>` | Close a BLE connection |
| `bt.list` |  | List BLE connections |
| `bt.pairing` | `<on\|off>` | Enable or disable pairing mode |
| `bt.iocap` | `<none\|display\|yesno\|keyboard\|keydisplay>` | Set the I/O capabilities (none = Just Works) |
| `bt.security` | `<none\|unauth\|auth\|sc\|sconly>` | Set the required security level |
| `bt.defaults` | `[secure]` | No security, no I/O, pairing on (default, applied automatically before any GATT server setup), or `secure`: LE Secure Connections with fallback to legacy pairing, numeric comparison (auto-confirmed) |
| `bt.autoconfirm` | `<on\|off>` | Accept numeric-comparison pairing automatically (default on) |
| `bt.confirm` | `<yes\|no>` | Answer a waiting pairing request (autoconfirm off) |
| `bt.passkey` | `<digits\|deny>` | Answer a waiting passkey request |
| `bt.bonds` |  | List bonded devices |
| `bt.unbond` | `<addr\|all>` | Remove one or all bonds |
| `bt.profile.add` | `<name> <addr[,random]>` | Save a BLE device as a named profile in `ucxapp.ble` (current directory; max 16) |
| `bt.profile.list` |  | List saved BLE profiles |
| `bt.profile.remove` | `<name>` | Delete a BLE profile |
| `bt.profile.connect` | `<name>` | Connect to a saved BLE profile; it becomes the "last used" profile |
| `bt.profile.auto` |  | Connect to the last used profile, else scan and take the strongest known device |

```powershell
# Find a device, save it once, then reconnect by name
ucxapp COM13 --exec "bt.scan"
ucxapp COM13 --exec "bt.profile.add sensor 11:22:33:44:55:66,random"
ucxapp COM13 --exec "bt.profile.connect sensor"

# Secure pairing
ucxapp COM13 --exec "bt.defaults secure" --exec "bt.connect 11:22:33:44:55:66"
```

### GATT client and server

| Command | Arguments | Description |
|---|---|---|
| `gatt.services` | `[conn]` | Discover and list the remote services and characteristics |
| `gatt.chars` | `[conn] [service-index]` | List the remote characteristics |
| `gatt.read` | `[conn] <handle>` | Read a characteristic; known types are decoded |
| `gatt.write` | `[conn] <handle> <hex\|text:abc> [norsp]` | Write a characteristic |
| `gatt.writelong` | `[conn] <handle> <hex\|text:abc> [offset] [reliable] [more]` | Long write (prepare/execute) |
| `gatt.subscribe` | `[conn] <handle> [notify\|indicate\|off]` | Set the CCCD (assumed at value handle + 1) |
| `gatt.listen` | `[seconds\|off]` | Client examples keep listening after they subscribe: notifications are printed parsed and mirrored to the Events pane until `gatt.listen off` unsubscribes. Without arguments it shows the state; with seconds it waits (needed for `gatt.wps` and server examples) |
| `gatt.client.example.<svc>` | `[conn]` (`sps`: `[flow]`) | Discover a predefined service, read, decode and subscribe; `<svc>` is `hrs`, `cts`, `ess`, `lns`, `aio`, `bas`, `dis`, `nus`, `sps` or `wit` (WitMotion BLE IMU; decodes the 0x55 0x61 packets) |
| `gatt.send` | `[conn] <nus\|sps> <text>` | Send text to the remote NUS or SPS service |
| `gatt.credits` | `[conn] <1-127>` | Send SPS credits |
| `gatt.wps` | `[conn] <status\|scan\|connect\|forget\|config <ssid> <pw> [OPEN\|WPA2\|WPA3]\|raw <text>>` | Send a Wi-Fi provisioning command to a remote device and wait for the response |
| `gatt.server.example.<svc>` | `[seconds]` | Define a predefined local service (once per module restart); with seconds, simulate values for a connected client. `<svc>` is `hrs`, `cts`, `ess`, `lns`, `aio`, `bas`, `nus` or `sps` |
| `gatt.server.example.nus` / `.sps` / `.wps` | `[seconds]` | Nordic UART, u-blox Serial Port and Wi-Fi Provisioning servers; with seconds, serve a client for that long (client writes are printed, WPS commands are executed) |
| `gatt.server.list` |  | List the local characteristics defined by the server examples |
| `gatt.server.set` | `<name\|handle> <hex\|text:abc>` | Set a local characteristic value |
| `gatt.server.notify` | `[conn] <name\|handle> <data>` | Send a notification |
| `gatt.server.indicate` | `[conn] <name\|handle> <data>` | Send an indication |
| `gatt.server.demo` | `[conn] <hrs\|bas\|ess\|aio\|lns\|cts> [seconds]` | Send simulated values once per second |
| `gatt.server.servicechanged` | `[conn] <start> <end>` | Send a Service Changed indication |
| `gatt.server.error` | `[conn] <read\|write> <att-code>` | Answer a pending read or write with an ATT error |
| `gatt.server.hostchar` | `<svc-uuid> <char-uuid> <props-hex> [rsec] [wsec]` | Define a host-managed characteristic |
| `gatt.server.readresp` / `gatt.server.writeresp` | `<conn> <data>` / `<conn>` | Answer a pending host-characteristic read or write |
| `hid.setup` |  | Define the HID keyboard service (plus battery) and enable Just Works pairing |
| `hid.status` |  | Show HID handles, notification state and protocol mode |
| `hid.key` | `<key> [ctrl\|shift\|alt\|gui ...]` | Send one key to the connected host (a character, or `enter`, `esc`, `tab`, `backspace`, `space`) |
| `hid.text` | `<text...>` | Type text on the connected host |

```powershell
# Client: read a heart rate sensor (connection handle 0 from bt.connect)
ucxapp COM13 --exec "bt.connect 11:22:33:44:55:66,random" --exec "gatt.client.example.hrs 0" --exec "gatt.listen 20"

# Server: act as a battery service and push simulated values for 60 s once a phone connects
ucxapp COM13 --exec "gatt.server.example.bas 60"

# Send text to a Nordic UART peer
ucxapp COM13 --exec "gatt.client.example.nus 0" --exec "gatt.send 0 nus hello"

# Use the module as a Bluetooth keyboard
ucxapp COM13 --exec "hid.setup" --exec "hid.text Hello from NORA"
```

### Wi-Fi

| Command | Arguments | Description |
|---|---|---|
| `wifi.scan` | | List networks strongest first with BSSID, channel and band, signal, security (Open, WPA, WPA2, WPA3) and any saved profile; then channel usage with congestion and the quietest channel for `wifi.ap.enable`. The results are kept for `wifi.connect` |
| `wifi.connect` | `[ssid\|n] [password]` | With no argument: connect like `wifi.profile.auto` (the profile of the network the PC is on, else last used, else the strongest known network). Otherwise connect by SSID or by the number from the last `wifi.scan`; waits up to 20 s and shows IP, mask, gateway, channel and signal. A saved profile for the SSID supplies the password; open networks need none |
| `wifi.status` | | Connection state, SSID, RSSI, IP |
| `wifi.disconnect` | | Leave the network |
| `wifi.ip` | `[dhcp \| <ip> <mask> <gw> [dns1 [dns2]]]` | Show, or set, DHCP or static IP; applies on the next `wifi.connect` |
| `wifi.rssi` | `[count] [interval_s]` | Read the signal strength, repeatedly if requested |
| `wifi.roaming` | `[on\|off]` | Show or set roaming between access points |
| `wifi.regdomain` | `[world\|etsi\|fcc\|ic\|nz\|mkk\|ncc\|acma\|kcc\|sa\|br]` | Show, or set and save, the regulatory domain used by `wifi.profile.auto` (default `etsi`) |
| `wifi.profile.add` | `<name> <ssid> [password]` | Save or update a Wi-Fi profile in `ucxapp.wifi` (current directory; passwords are obfuscated, not encrypted; max 16) |
| `wifi.profile.list` |  | List saved profiles (no passwords shown) and the network (IP prefix) each was last used on |
| `wifi.profile.remove` | `<name>` | Delete a saved profile |
| `wifi.profile.connect` | `<name>` | Connect with a saved profile; it becomes the "last used" profile |
| `wifi.profile.auto` | | Connect to the profile of the network the PC is on (matched by IP prefix, learned the first time a profile connects); else try the last used profile (10 s); else scan with the saved regulatory domain and connect to the strongest known profile. Keeps an existing link to the same SSID |
| `wifi.ap.enable` | `[ssid] [password] [channel]` | Start a WPA2 access point and show its join QR code |
| `wifi.ap.disable` |  | Stop the access point |
| `wifi.ap.status` |  | Show AP parameters, addresses and stations |

```powershell
# Connect once
ucxapp COM13 --exec "wifi.connect MySsid MyPassword" --exec "wifi.status"

# Save networks (home, office), then let the app pick the best one anywhere
ucxapp COM13 --exec "wifi.profile.add home MySsid MyPassword"
ucxapp COM13 --exec "wifi.profile.add office CorpNet secret"
ucxapp COM13 --exec "wifi.profile.auto"

# Static IP, applied on the next connect
ucxapp COM13 --exec "wifi.ip 192.168.1.50 255.255.255.0 192.168.1.1 8.8.8.8" --exec "wifi.connect MySsid MyPassword"

# Access point with a join QR code
ucxapp COM13 --exec "wifi.ap.enable NORA-Test secret123 6"
```

Passwords given on the command line are visible in the process list and shell history.

### Network

| Command | Arguments | Description |
|---|---|---|
| `net.ping` | `<host> [count]` | ICMP ping, count 1-10 (default 4); fails if no reply |
| `net.iperf` | `<server\|client\|stop> [args...]` | iperf throughput test run by the module: `server [tcp\|udp] [port]` keeps running until `stop`; `client <ip> [tcp\|udp] [seconds] [port]` runs for the given time (default 10 s, port 5001). Needs a Wi-Fi connection |
| `net.dns` | `<host>` | Resolve a host name |

```powershell
ucxapp COM13 --exec "wifi.profile.auto" --exec "net.dns example.com" --exec "net.ping example.com 3"
```

### Sockets

| Command | Arguments | Description |
|---|---|---|
| `sock.open` | `<tcp\|udp>` | Create a socket and print its handle |
| `sock.connect` | `[handle] <host> <port>` | Connect; waits up to 10 s |
| `sock.send` | `[handle] <text...>` | Send text (arguments are joined with single spaces, max 511 bytes; `\r`, `\n` and `\\` are converted) |
| `sock.read` | `[handle] [max] [wait_s]` | Wait for data (default 5 s) and print up to `max` bytes (default and maximum 512) |
| `sock.bind` | `[handle] <port>` | Bind a socket to a local port (UDP receive) |
| `sock.listen` | `[handle] [port]` | Listen for TCP connections |
| `sock.accept` | `[wait_s]` | Wait for an incoming TCP connection (default 30 s) and print its handle |
| `sock.sendto` | `[handle] <host> <port> <text...>` | Send a UDP datagram |
| `sock.recvfrom` | `[handle] [max] [wait_s]` | Receive a UDP datagram and print the sender |
| `sock.close` | `[handle]` | Close the socket |
| `sock.list` | | List open sockets |

```powershell
# Fetch a page over TCP
ucxapp COM13 `
  --exec "wifi.connect MySsid MyPassword" `
  --exec "sock.open tcp" `
  --exec "sock.connect 0 example.com 80" `
  --exec "sock.send 0 GET / HTTP/1.0\r\n\r\n" `
  --exec "sock.read 0 200 5" `
  --exec "sock.close 0"

# Send a UDP datagram
ucxapp COM13 --exec "sock.open udp" --exec "sock.sendto 0 192.168.1.20 5000 hello"
```

### MQTT

| Command | Arguments | Description |
|---|---|---|
| `mqtt.connect` | `[host] [port]` | Connect to an MQTT broker (default `broker.emqx.io:1883`, no TLS); waits up to 10 s |
| `mqtt.disconnect` | | Disconnect from the broker |
| `mqtt.subscribe` | `<topic> [qos]` | Subscribe (wildcards allowed, qos 0-2) |
| `mqtt.unsubscribe` | `<topic>` | Unsubscribe |
| `mqtt.publish` | `<topic> <text...>` | Publish a QoS 0, non-retained message (max 511 bytes) |
| `mqtt.read` | `[wait_s]` | Wait (default 10 s) for messages and print them |

```powershell
ucxapp COM13 --exec "wifi.profile.auto" --exec "mqtt.connect" `
  --exec "mqtt.subscribe ucx/demo/#" --exec "mqtt.publish ucx/demo/hello hi there" --exec "mqtt.read 5"
```

Known firmware behaviour: on NORA-W36 an MQTT publish made while a subscription is active returned ERROR:15 in testing, and back-to-back MQTT operations get ERROR:15 while the previous one is in flight; the commands retry the latter automatically.

### HTTP

| Command | Arguments | Description |
|---|---|---|
| `http.get` | `<url> [show_bytes]` | GET over HTTP or HTTPS (scheme optional, default HTTP); prints the status line and up to `show_bytes` (default 512) of the body |
| `http.download` | `<url> <file>` | Download to a file, following up to 5 redirects in the app (the module firmware does not); needs a 2xx response, checks `Content-Length` when present, deletes the file on failure |

```powershell
ucxapp COM13 --exec "wifi.profile.auto" --exec "http.get https://example.com 300"
ucxapp COM13 --exec "wifi.profile.auto" --exec "http.download https://example.com/file.bin file.bin"
```

REST and geolocation examples (the module needs Wi-Fi; replies are JSON text, so raise `show_bytes`; not run against
the services here, which may change or rate-limit):

```powershell
# Position from the public IP address, then the QR code for it (copy lat/lon from the reply)
ucxapp COM13 --exec "wifi.profile.auto" --exec "http.get http://ip-api.com/json 600"
ucxapp COM13 --exec "utils.qr.geo 59.33 18.07"
# Any REST endpoint works the same way
ucxapp COM13 --exec "wifi.profile.auto" --exec "http.get https://api.github.com/zen 200"
```

NORA-W36 has no GNSS; for a position from a receiver on the PC use `gnss.open` and `gnss.status`.

### Security

| Command | Arguments | Description |
|---|---|---|
| `cert.list` | | List installed certificates and keys with their type (`ca`, `client`, `key`) |
| `cert.info` | `<name>` | Size and SHA-256 fingerprint |
| `cert.upload` | `<ca\|client\|key> <file> [name] [pw]` | Upload a PEM file (max 8192 bytes); the name defaults to the file name, `pw` is for encrypted keys. For `ca` and `client` the fingerprint is checked against the module afterwards |
| `cert.remove` | `<name>` | Remove a certificate or key (type is looked up automatically) |

```powershell
ucxapp COM13 --exec "cert.upload ca root-ca.pem" --exec "cert.upload client device.pem" --exec "cert.upload key device-key.pem" --exec "cert.list"
```

### GNSS and IMU sensors (PC only, no module needed except for bridges)

A sensor on a PC serial port is read by a background thread. `gnss.*` handles u-blox receivers (NMEA and UBX); `imu.*` handles WitMotion serial IMUs (WT61, WT901 and similar, including the 20-byte BLE packet) and Bosch BNO055 over UART. Bosch BMI160/BMI270/BMI323 and BNO08x have no PC serial interface (I2C or SPI only) and are not supported.

A bridge forwards the raw byte stream through the connected module: `tcp` listens on the module's IP address (up to 4 clients), `udp` sends datagrams, `ble` notifies on the Nordic UART TX characteristic (needs a connected client with notifications enabled). The module must be on Wi-Fi for `tcp` and `udp`. The AT link limits throughput to a few kB/s; excess data is dropped and counted.

| Command | Arguments | Description |
|---|---|---|
| `gnss.detect` | `[port]` | Find a GNSS receiver on the serial ports (tries 9600 to 921600 baud) |
| `gnss.open` | `[port] [baud]` | Open a receiver; detects port and baud when omitted |
| `gnss.close` | | Close the receiver |
| `gnss.status` | | Fix, position, time, satellites, DOP, receiver version and counters |
| `gnss.sats` | | Satellites with elevation, azimuth, signal strength and used flag |
| `gnss.log` | `<file\|off>` | Record the raw receiver stream to a file |
| `gnss.send` | `<hex..\|$NMEA\|ubx cls id [hex]>` | Send raw bytes, an NMEA sentence (checksum added) or a UBX frame |
| `gnss.restart` | `<hot\|warm\|cold>` | Restart the receiver (UBX-CFG-RST) |
| `gnss.rate` | `<hz>` | Navigation rate 1 to 25 Hz (UBX-CFG-RATE) |
| `gnss.bridge` | `<tcp [port]\|tcpc <host> <port>\|udp <host> <port> [local]\|udps <port>\|ble [nus\|sps\|custom <uuid> [rx tx]]\|uart <port> [baud]\|cell\|off\|status>` | Two-way bridge of the receiver stream. `tcp` serves (default 2947), `tcpc` connects and reconnects, `udp` sends and listens on `local`, `udps` listens and answers the last sender, `ble` is Nordic UART (`nus`), u-blox SPS or your own service (`custom`, below), `uart` a second PC port, `cell` the data link opened by `cell.connect`. Data from the far side goes to the receiver |
| `imu.detect` | `[port]` | Find a WitMotion or Bosch BNO055 IMU |
| `imu.open` | `[port] [baud] [wit\|bno055]` | Open an IMU; detects port, baud and type when omitted |
| `imu.close` | | Close the IMU |
| `imu.status` | | Accel, gyro, angle, magnetometer, quaternion, pressure, steps and counters |
| `imu.log` | `<file\|off>` | Record the raw IMU byte stream to a file |
| `imu.send` | `<hex..>` | Send raw bytes (WitMotion configuration frames) |
| `imu.rate` | `<hz>` | WitMotion output rate (0.2 to 200 Hz) |
| `imu.bridge` | `<tcp [port]\|tcpc <host> <port>\|udp <host> <port> [local]\|udps <port>\|ble [nus\|sps\|custom <uuid> [rx tx]]\|uart <port> [baud]\|cell\|off\|status>` | Two-way bridge of the IMU stream, same modes as `gnss.bridge` |

### Cellular modules

Basic access to a cellular module on a PC serial port, using the generic 3GPP AT commands (TS 27.007) that
Quectel, SIMCom, u-blox, Telit, Sierra and Nordic nRF91 modules share. Not verified on hardware yet; only
`cell.ping` and `cell.connect` are vendor specific (picked from the module's ATI maker string). Everything else goes through `cell.at`.

| Command | Arguments | Description |
|---|---|---|
| `cell.detect` | `[port]` | Find a module answering AT (115200, 9600, 921600, 460800 baud) |
| `cell.open` | `<port> [baud]` | Open a module (default 115200) |
| `cell.close` |  | Close it |
| `cell.status` |  | SIM, ICCID, signal in dBm, registration (LTE and 2G/3G), operator, IP |
| `cell.at` | `<command>` | Send a raw AT command |
| `cell.apn` | `[apn [ip\|ipv6\|ipv4v6]]` | Show or set the APN of context 1 |
| `cell.attach` | `[on\|off]` | Attach and activate data, detach, or show the state |
| `cell.ping` | `<host>` | Ping over the cellular link (Quectel and u-blox only) |
| `cell.bridge` | `<tcp [port]\|tcpc <host> <port>\|udp <host> <port> [local]\|udps <port>\|ble [nus\|sps\|custom <uuid> [rx tx]]\|uart <port> [baud]\|off\|status>` | Give the modem's AT console to a peer, same modes as `gnss.bridge` (`off` returns to command mode) |
| `cell.connect` | `<host> <port> [tcp\|udp]` | Open a transparent socket through the modem. Vendor paths: u-blox (default, `AT+USOCR`/`AT+USOCO`), Quectel (`AT+QIOPEN` access mode 2), SIMCom (`AT+CIPMODE=1`, `AT+CIPOPEN`), Telit (`AT#SGACT`, `AT#SD`), Nordic nRF91 (`AT#XSOCKET`, `AT#XCONNECT`). The modem's UART then carries the payload: bridge to it with `gnss.bridge cell` or `imu.bridge cell` |
| `cell.disconnect` | | Stop the bridge using the link first; sends `+++` after the guard time and closes the socket |

Custom BLE service: `ble custom <uuid>` defines a 128-bit service with a write characteristic (RX, bytes go to the
receiver) and a notify characteristic (TX). With only the service UUID, RX and TX are the service UUID with the fourth
byte increased by 1 and 2 (Nordic style); give `<rx> <tx>` UUIDs to choose them. UUIDs are 32 hex digits, dashes optional.
The module keeps a service until it restarts, so one custom service per restart. Cellular transport is not verified on
hardware and is mapped for Quectel modules only; the `+QIOPEN` timing and `+++` guard time may need adjusting.

### Hardware I/O (hw.*)

GPIO, I2C, SPI, ADC, PWM and CAN behind one backend. On Windows and Linux an emulated backend runs inside the app
(16 pins, I2C devices at 0x48/0x68/0x76, SPI MISO wired to MOSI, ADC triangle waves, CAN loopback), so scripts and the
bridge can be developed without a board. Writing to a pin in `in` mode sets its simulated level. Real STM32 drivers are
not wired in yet.

| Command | Arguments | Description |
|---|---|---|
| `hw.status` |  | Backend and peripheral summary |
| `hw.load` | `<file.json>` | Load a board profile: pin names, channel counts and emulated I2C devices (shell only) |
| `hw.board` |  | Pin map of the loaded profile |
| `hw.gpio.mode` | `<pin> <in\|out\|pullup\|pulldown>` | Set a GPIO pin mode |
| `hw.gpio.write` | `<pin> <0\|1>` | Drive an output (or simulate an input level) |
| `hw.gpio.read` | `<pin>` | Read a GPIO pin |
| `hw.gpio.toggle` | `<pin>` | Invert a GPIO output |
| `hw.gpio.list` |  | Levels of all GPIO pins |
| `hw.led` | `<pin> <on\|off\|toggle>` | Drive a user LED; configures the pin as an output (pins can be given by name) |
| `hw.i2c.scan` |  | Scan the I2C bus |
| `hw.i2c.write` | `<addr> <hex...>` | Write bytes to a device |
| `hw.i2c.read` | `<addr> <len>` | Read bytes from a device |
| `hw.i2c.reg` | `<addr> <reg> [len]` | Read a register |
| `hw.spi.cfg` | `<mode 0-3> <hz>` | Set the SPI mode and clock |
| `hw.spi.xfer` | `<hex...>` | Full duplex transfer, prints the bytes read |
| `hw.adc.read` | `<ch>` | Read an ADC channel (12 bit, 3.3 V reference) |
| `hw.pwm.set` | `<ch> <hz> <duty%>` | Start PWM on a channel |
| `hw.pwm.off` | `<ch>` | Stop PWM on a channel |
| `hw.can.cfg` | `<bitrate>` | Set the CAN bit rate |
| `hw.can.send` | `<id> [hex...]` | Send a CAN frame (id above 0x7FF is extended) |
| `hw.can.recv` |  | Take the next received frame |
| `hw.exec` | `<command...>` | Run one bridge line, for example `hw.exec gpio.read 3`, and print the reply |
| `hw.bridge` | `<tcp [port]\|tcpc <host> <port>\|udp <host> <port> [local]\|udps <port>\|ble [nus\|sps\|custom <uuid> [rx tx]]\|uart <port> [baud]\|cell\|off\|status>` | Same modes as `gnss.bridge` (TCP default port 7000) |

The bridge speaks lines: the peer sends a command without the `hw.` prefix (`i2c.reg 0x68 0x75`) and gets `OK <text>`
or `ERR <text>`. While bridged, GPIO input changes arrive as `EVT gpio <pin> <level>` and received CAN frames as
`EVT can <id> <hex>` instead of through `hw.can.recv`.

Board profiles live in `app/boards/` (`h753.json`, `f767.json`, `f439.json`); `scripts/hw_config.py <profile>
--out hw_config.h` validates one (unknown keys, bad or duplicate pins, reserved pins) and generates X-macro pin tables
for a firmware build. `hw.load` reads the same file on the host. Pins can then be given by name (`hw.gpio.write PB0 1`).
Format: `board`, `gpio[]`, `adc[]`, `pwm[{pin,timer}]`, `i2c{scl,sda}`, `spi{sck,miso,mosi,cs}`, `can{rx,tx}`,
`reserved[]` and, for the emulation only, `emu.i2c[{addr,regs{reg:value}}]`.

Default pins on all three Nucleo-144 boards (not verified on hardware; the STM32 firmware does not use them yet):
I2C1 PB8/PB9, SPI4 PE2 (SCK), PE5 (MISO), PE6 (MOSI), PE4 (CS), CAN on PD0 (RX) and PD1 (TX), PWM on TIM1 PE9/PE11/PE13/PE14,
ADC on PA3, PC0, PC3, PF10, GPIO on the user LEDs and button plus PF12-PF15, PD14 and PD15. PB6/PB7, PG9/PG14 and
PD8/PD9 stay reserved. SPI4 is used because PA7 on SPI1 is shared with the Ethernet PHY.

### Tools (no AT equivalent)

| Command | Arguments | Description |
|---|---|---|
| `utils.qr.text` | `<text...>` | Print a QR code for any text |
| `utils.qr.wifi` | `<ssid> [password] [WPA\|WEP\|nopass] [hidden]` | Print a Wi-Fi join QR code |
| `utils.qr.geo` | `<lat> <lon>` | Print a location QR code |
| `utils.zip` | `<archive.zip> <file> [file...]` | Create a zip archive from up to 6 files (deflate; no device needed) |
| `utils.unzip` | `<archive.zip> [dir]` | Extract a zip archive, default into the current directory (no device needed) |
| `log.at.on` | | Show the AT TX/RX trace |
| `log.at.off` | | Hide the AT TX/RX trace (default) |
| `log.warn.on` | | Show library warnings such as `Unexpected data` (refused in the text UI, where they would garble the screen) |
| `log.warn.off` | | Hide library warnings |
| `log.events.on` | | Also print module events (Wi-Fi, Bluetooth, MQTT ...) in the log as `[event] ...` |
| `log.events.off` | | Keep events out of the log (default); they still show in the Events pane |
| `log.level` | `<off\|warn\|all>` | One switch: `off` hides warnings, AT trace and events; `warn` shows warnings only; `all` shows warnings, AT trace and events |
| `log.save` | `<file>` | Save everything the log shows to a file; the file is written when `log.stop` runs |
| `log.stop` | | Finish and write the file started by `log.save` |
| `log.clear` | | Clear the log (in the text UI the Log pane) |
| `file.hash` | `<md5\|sha256> <file>` | Print the hash of a file (no device needed) |
| `file.view` | `<file> [lines]` | Show the first lines of a text file, read only (default 40 lines; non-printable bytes shown as dots) |
| `file.list` | | List the files in the working folder, numbered, with sizes |
| `file.remove` | `<file\|number>` | Delete a file by name or by its number from `file.list` |
| `stats.show` | `[tcp\|udp\|sps\|gatt\|http\|mqtt\|web] [rx\|tx]` | Payload bytes sent and received per channel; `stats.show tcp rx` prints just the number |
| `stats.clear` | `[tcp\|udp\|sps\|gatt\|http\|mqtt\|web]` | Reset one channel's counters, or all |
| `app.info` | | About this app: maker, platform and links |
| `app.version` | | App version, build time, platform and compiler |
| `app.uptime` | | Time since the app started (`days hh:mm:ss`) |
| `app.sbom` | | Software bill of materials: components, versions, licences and sources |
| `app.license` | `[name]` | Licence of each component; with a name (e.g. `qrcodegen`), the full notice |
| `web.start` | `[port] [token]` | Serve a web UI from the module's TCP listener (needs Wi-Fi; default port 80); prints `http://<ip>:<port>/?t=<token>` |
| `web.stop` |  | Stop the web server |
| `web.status` |  | Show whether the web server is running, its port and SSE client count |

```powershell
ucxapp COM13 --exec "utils.qr.wifi MySsid MyPassword WPA"
ucxapp COM13 --shell        # then: wifi.profile.auto, web.start
```

#### Web UI

`web.start` serves a page with four parts: a **Commands** panel generated from the command table
(one section per group, one form per command built from its usage string, so a new command shows up
without touching the page), a **Log** pane that mirrors everything the app prints (the
same text as the console), an **Events** pane that shows module events as they arrive (Wi-Fi link and
network up/down, MQTT, Bluetooth, GATT, SPS, HTTP and ping results), and a command line with history
(up/down arrows) and quick buttons. Commands run one at a time; the `web.*` commands are not allowed
from the page. Open the printed URL, including the `?t=` token.

#### Text UI

`--tui` opens the same three panes in a terminal window (Windows console, Linux terminal) and is the
default when there is no `--exec` or `--shell`. The layout widens with the window up to 180 columns by 45
rows; the first Log line shows the app version and build date (`--version` prints the same). Log lines wrap at word boundaries.
**Commands** (groups in AT manual order; Right/Enter expands a group or puts a command on the input line),
**Log** and **Events** (colored by source), with a hint line that shows the usage of the selected or
typed command.

```powershell
ucxapp COM13 --tui
```

| Key | Action |
|---|---|
| Enter | Run the input line (`exit` or `quit` leaves, `clear` empties the log) |
| Up / Down | Input history; in the Commands pane, move the selection |
| Tab | Complete the command name |
| Esc | Switch between the input line and the Commands pane |
| PgUp / PgDn | Scroll the log |
| F10, Ctrl-C | Quit |

The text UI and the web page share one core (`ucxapp_ui.c`): the command table and its usage parsing, the
log and event streams and the one-at-a-time command runner. A command added to a pack appears in both.

## Exit codes and errors

Commands return `UCXAPP_OK` (0) or a negative code (`src/ucxapp_cmd.h`):

| Code | Meaning |
|---|---|
| `-100` | Unknown command |
| `-101` | Bad arguments; the usage line is printed |
| `-102` | Command needs a module and none is connected |
| `-103` | The module reported a failure or a wait timed out |

In `--exec` mode the process exits with 1 on the first non-zero result, otherwise 0.

## Architecture

| File | Role |
|---|---|
| `ucxapp.h` | `ucxapp_t` context, IO layer, URC event flags |
| `ucxapp_cmd.h/.c` | Command registry, tokenizer, `help`, shell loop |
| `ucxapp_io.c`, `ucxapp_io_stdio.c` | `ucxapp_print` / `ucxapp_readline` and the stdio backend |
| `ucxapp_events.c` | URC event flags with a mutex; `ucxapp_events_wait()` blocks until a flag is set |
| `ucxapp_cmds_device.c`, `_wifi.c`, `_sock.c`, `_net.c`, `_sys.c`, `_mqtt.c`, `_bt.c` | Command packs |
| `ucxapp_platform.h` | Host OS services: console init, tick, key press, executable directory, serial-port listing, HTTPS GET |
| `ucxapp_platform_win.c` | Windows backend (registry, SetupAPI, FTDI D2XX, WinHTTP, `conio`) |

Rules for portable code: use `ucxapp_print()` / `ucxapp_readline()` for all I/O, and only
u-connectClient APIs and its `U_CX_PORT_*` macros for OS services.

## Adding a command

A command is a table entry. Register the table once at startup.

```c
static int cmdHello(ucxapp_t *app, int argc, char *argv[])
{
    /* argv[0] is the command name; use app->ucx for the module */
    ucxapp_print("hello\n");
    return UCXAPP_OK;            /* or UCXAPP_ERR_USAGE / UCXAPP_ERR_FAILED */
}

static const ucxapp_cmd_t gMyCmds[] = {
    { "my.hello", "[name]", "Say hello", UCXAPP_CMD_NEEDS_DEVICE, cmdHello },
};

void ucxapp_register_my_cmds(void)
{
    ucxapp_cmd_register(gMyCmds, sizeof(gMyCmds) / sizeof(gMyCmds[0]));
}
```

Then call the register function in `main()` and add the file to `CMakeLists.txt`.
`UCXAPP_CMD_NEEDS_DEVICE` makes the dispatcher reject the command when no module is connected,
so the handler can use `app->ucx` without checking. Returning `UCXAPP_ERR_USAGE` prints
`usage: <name> <usage>` automatically. To wait for a URC use
`ucxapp_events_clear(flag)` before the AT command and `ucxapp_events_wait(flag, seconds)` after it.

## Porting to another target

1. Provide a `ucxapp_io_t` (`write` and `readline`, for example over a UART) and pass it to `ucxapp_io_set()`.
2. Create the `uCxHandle_t` with the u-connectClient port for your platform and set `app.ucx`.
3. Call `ucxapp_events_init()` and register the URC handlers that call `ucxapp_events_signal()`.
4. Register the command packs and call `ucxapp_shell_run(&app)` or `ucxapp_cmd_exec()`.

Steps 2 and 3 are done by `src/ucxapp_main.c` (and `src/ucxapp_urc.c`), which is the entry point on both Windows and POSIX.
