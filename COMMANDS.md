# Command Layer

A small, portable layer for driving a u-connectXpress module with text commands instead of the
menu. It lives in `app/src/ucxapp_*` and has no Windows dependencies, so the same
commands can run in a PC console, a Linux terminal or a UART shell on an embedded target.

## Using it

```powershell
# One-shot: run commands in order, stop at the first failure (exit code 1)
u-connectClientApp.exe COM13 --exec "device.info" --exec "wifi.scan"

# Interactive shell
u-connectClientApp.exe COM13 --shell
```

In the normal menu, press `:` to open the same shell; `exit` returns to the menu.

- Without a port argument, `--exec` and `--shell` use the saved port from the ini file.
- If the module cannot be connected, commands that need it fail with `not connected`; `help` still works.
- Arguments are whitespace separated; use double quotes for values with spaces
  (`wifi.connect "My Network" "my password"`). `\"` and `\\` escape inside quotes.
- Lines starting with `#` are comments. Maximum 8 arguments per command.

## Command reference

| Command | Arguments | Description |
|---|---|---|
| `help` | `[prefix]` | List commands, optionally filtered (`help wifi`) |
| `device.info` | | Manufacturer, model, firmware version |
| `device.reboot` | | Reboot the module (clears GATT services and connections); use as the last command |
| `device.connect` | `[port]` | Connect to the module (default: saved port). Windows only |
| `device.disconnect` | | Disconnect. Windows only |
| `wifi.scan` | | List networks with BSSID, channel, RSSI, security |
| `wifi.connect` | `<ssid> [password]` | Connect to a network; waits up to 20 s for an IP. Omit the password for open networks |
| `wifi.status` | | Connection state, SSID, RSSI, IP |
| `wifi.disconnect` | | Leave the network |
| `wifi.ip` | `[dhcp \| <ip> <mask> <gw> [dns1 [dns2]]]` | Show, or set, DHCP or static IP; applies on the next `wifi.connect` |
| `wifi.rssi` | `[count] [interval_s]` | Read the signal strength, repeatedly if requested |
| `wifi.profile.add` | `<name> <ssid> [password]` | Save or update a Wi-Fi profile in `u-connectClientApp.wifi` (current directory; passwords are obfuscated, not encrypted; max 16) |
| `wifi.profile.list` |  | List saved profiles (no passwords shown) |
| `wifi.profile.remove` | `<name>` | Delete a saved profile |
| `wifi.profile.connect` | `<name>` | Connect with a saved profile; it becomes the "last used" profile |
| `wifi.profile.auto` | | Try the last used profile (10 s); if that fails, scan with the saved regulatory domain and connect to the strongest known profile, which becomes the last used |
| `wifi.regdomain` | `[world\|etsi\|fcc\|ic\|nz\|mkk\|ncc\|acma\|kcc\|sa\|br]` | Show, or set and save, the regulatory domain used by `wifi.profile.auto` (default `etsi`) |
| `sock.open` | `<tcp\|udp>` | Create a socket and print its handle |
| `sock.connect` | `<handle> <host> <port>` | Connect; waits up to 10 s |
| `sock.send` | `<handle> <text...>` | Send text (arguments are joined with single spaces, max 511 bytes; `\r`, `\n` and `\\` are converted) |
| `sock.read` | `<handle> [max] [wait_s]` | Wait for data (default 5 s) and print up to `max` bytes (default and maximum 512) |
| `sock.bind` | `<handle> <port>` | Bind a socket to a local port (UDP receive) |
| `sock.listen` | `<handle> [port]` | Listen for TCP connections |
| `sock.accept` | `[wait_s]` | Wait for an incoming TCP connection (default 30 s) and print its handle |
| `sock.sendto` | `<handle> <host> <port> <text...>` | Send a UDP datagram |
| `sock.recvfrom` | `<handle> [max] [wait_s]` | Receive a UDP datagram and print the sender |
| `sock.close` | `<handle>` | Close the socket |
| `sock.list` | | List open sockets |
| `net.ping` | `<host> [count]` | ICMP ping, count 1-10 (default 4); fails if no reply |
| `net.dns` | `<host>` | Resolve a host name |
| `http.get` | `<url> [show_bytes]` | GET over HTTP or HTTPS (scheme optional, default HTTP); prints the status line and up to `show_bytes` (default 512) of the body |
| `http.download` | `<url> <file>` | Download to a file, following up to 5 redirects in the app (the module firmware does not); needs a 2xx response, checks `Content-Length` when present, deletes the file on failure |
| `mqtt.connect` | `[host] [port]` | Connect to an MQTT broker (default `broker.emqx.io:1883`, no TLS); waits up to 10 s |
| `mqtt.disconnect` | | Disconnect from the broker |
| `mqtt.subscribe` | `<topic> [qos]` | Subscribe (wildcards allowed, qos 0-2) |
| `mqtt.unsubscribe` | `<topic>` | Unsubscribe |
| `mqtt.publish` | `<topic> <text...>` | Publish a QoS 0, non-retained message (max 511 bytes) |
| `mqtt.read` | `[wait_s]` | Wait (default 10 s) for messages and print them |
| `bt.scan` |  | Scan for BLE devices (up to 30 s) |
| `bt.connect` | `<addr[,random]>` | Connect to a BLE device, e.g. 11:22:33:44:55:66 |
| `bt.profile.add` | `<name> <addr[,random]>` | Save a BLE device as a named profile in `u-connectClientApp.ble` (current directory; max 16) |
| `bt.profile.list` |  | List saved BLE profiles |
| `bt.profile.remove` | `<name>` | Delete a BLE profile |
| `bt.profile.connect` | `<name>` | Connect to a saved BLE profile; it becomes the "last used" profile |
| `bt.profile.auto` |  | Connect to the last used profile, else scan and take the strongest known device |
| `bt.disconnect` | `<handle>` | Close a BLE connection |
| `bt.list` |  | List BLE connections |
| `bt.pairing` | `<on|off>` | Enable or disable pairing mode |
| `bt.iocap` | `<none|display|yesno|keyboard|keydisplay>` | Set the I/O capabilities (none = Just Works) |
| `bt.security` | `<none|unauth|auth|sc|sconly>` | Set the required security level |
| `bt.defaults` | `[secure]` | No security, no I/O, pairing on (default, applied automatically before any GATT server setup), or `secure`: LE Secure Connections with fallback to legacy pairing, numeric comparison (auto-confirmed) |
| `bt.autoconfirm` | `<on|off>` | Accept numeric-comparison pairing automatically (default on) |
| `bt.confirm` | `<yes|no>` | Answer a waiting pairing request (autoconfirm off) |
| `bt.passkey` | `<digits|deny>` | Answer a waiting passkey request |
| `bt.bonds` |  | List bonded devices |
| `bt.unbond` | `<addr|all>` | Remove one or all bonds |
| `gatt.services` | `<conn>` | Discover and list the remote services and characteristics |
| `gatt.chars` | `<conn> [service-index]` | List the remote characteristics |
| `gatt.read` | `<conn> <handle>` | Read a characteristic; known types are decoded |
| `gatt.write` | `<conn> <handle> <hex\|text:abc> [norsp]` | Write a characteristic |
| `gatt.subscribe` | `<conn> <handle> [notify\|indicate\|off]` | Set the CCCD (assumed at value handle + 1) |
| `gatt.client.example.<svc>` | `<conn>` (`sps`: `[flow]`) | Discover a predefined service, read, decode and subscribe; `<svc>` is `hrs`, `cts`, `ess`, `lns`, `aio`, `bas`, `dis`, `nus` or `sps` |
| `gatt.send` | `<conn> <nus\|sps> <text>` | Send text to the remote NUS or SPS service |
| `gatt.credits` | `<conn> <1-127>` | Send SPS credits |
| `gatt.listen` | `[seconds]` | Wait and print incoming notifications (default 10 s) |
| `gatt.server.example.<svc>` | `[seconds]` | Define a predefined local service (once per module restart); with seconds, simulate values for a connected client. `<svc>` is `hrs`, `cts`, `ess`, `lns`, `aio`, `bas`, `nus` or `sps` |
| `gatt.server.list` |  | List the local characteristics defined by the server examples |
| `gatt.server.set` | `<name\|handle> <hex\|text:abc>` | Set a local characteristic value |
| `gatt.server.notify` | `<conn> <name\|handle> <data>` | Send a notification |
| `gatt.server.indicate` | `<conn> <name\|handle> <data>` | Send an indication |
| `gatt.server.demo` | `<conn> <hrs\|bas\|ess\|aio\|lns\|cts> [seconds]` | Send simulated values once per second |
| `gatt.server.example.nus` / `.sps` / `.wps` | `[seconds]` | Nordic UART, u-blox Serial Port and Wi-Fi Provisioning servers; with seconds, serve a client for that long (client writes are printed, WPS commands are executed) |
| `gatt.wps` | `<conn> <status\|scan\|connect\|forget\|config <ssid> <pw> [OPEN\|WPA2\|WPA3]\|raw <text>>` | Send a Wi-Fi provisioning command to a remote device and wait for the response |
| `gatt.writelong` | `<conn> <handle> <hex\|text:abc> [offset] [reliable] [more]` | Long write (prepare/execute) |
| `gatt.server.servicechanged` | `<conn> <start> <end>` | Send a Service Changed indication |
| `gatt.server.error` | `<conn> <read\|write> <att-code>` | Answer a pending read or write with an ATT error |
| `gatt.server.hostchar` | `<svc-uuid> <char-uuid> <props-hex> [rsec] [wsec]` | Define a host-managed characteristic |
| `gatt.server.readresp` / `gatt.server.writeresp` | `<conn> <data>` / `<conn>` | Answer a pending host-characteristic read or write |
| `hid.setup` |  | Define the HID keyboard service (plus battery) and enable Just Works pairing |
| `hid.status` |  | Show HID handles, notification state and protocol mode |
| `hid.key` | `<key> [ctrl\|shift\|alt\|gui ...]` | Send one key to the connected host (a character, or `enter`, `esc`, `tab`, `backspace`, `space`) |
| `hid.text` | `<text...>` | Type text on the connected host |
| `qr.text` | `<text...>` | Print a QR code for any text |
| `qr.wifi` | `<ssid> [password] [WPA\|WEP\|nopass] [hidden]` | Print a Wi-Fi join QR code |
| `qr.geo` | `<lat> <lon>` | Print a location QR code |
| `wifi.ap.enable` | `[ssid] [password] [channel]` | Start a WPA2 access point and show its join QR code |
| `wifi.ap.disable` |  | Stop the access point |
| `wifi.ap.status` |  | Show AP parameters, addresses and stations |
| `file.hash` | `<md5\|sha256> <file>` | Print the hash of a file (no device needed) |
| `web.start` | `[port] [token]` | Serve a web UI from the module's TCP listener (needs Wi-Fi; default port 80); prints `http://<ip>:<port>/?t=<token>` |
| `web.stop` |  | Stop the web server |
| `web.status` |  | Show whether the web server is running, its port and SSE client count |
| `cert.list` | | List installed certificates and keys with their type (`ca`, `client`, `key`) |
| `cert.info` | `<name>` | Size and SHA-256 fingerprint |
| `cert.upload` | `<ca\|client\|key> <file> [name] [pw]` | Upload a PEM file (max 8192 bytes); the name defaults to the file name, `pw` is for encrypted keys. For `ca` and `client` the fingerprint is checked against the module afterwards |
| `cert.remove` | `<name>` | Remove a certificate or key (type is looked up automatically) |
| `fw.download` | `<url> <file> [sha256]` or `latest [file]` | Download via the module (follows redirects), print SHA-256, delete the file on mismatch. `latest` looks up the newest u-connectXpress GitHub release for the connected module family (e.g. NORA-W36) and saves its zip (Windows only); flash it with `fw.flash` |
| `fw.flash` | `<file.bin\|file.zip> [baud]` | XMODEM firmware update (default 921600 baud); a zip is only extracted (needs `tar` on Windows, `unzip` elsewhere), and only the `.bin` inside is flashed |
| `at` | `<command>` | Send a raw AT command; the `AT` prefix is optional (`at +GMI`) |

Example: fetch a page over Wi-Fi.

```powershell
u-connectClientApp.exe COM13 `
  --exec "wifi.connect MySsid MyPassword" `
  --exec "sock.open tcp" `
  --exec "sock.connect 0 example.com 80" `
  --exec "sock.send 0 GET / HTTP/1.0" `
  --exec "sock.read 0 200 5" `
  --exec "sock.close 0"
```

Passwords given on the command line are visible in the process list and shell history.

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


Known firmware behaviour: on NORA-W36 an MQTT publish made while a subscription is active returned ERROR:15 in testing, and back-to-back MQTT operations get ERROR:15 while the previous one is in flight; the commands retry the latter automatically.

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
