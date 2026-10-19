# Command Layer

A small, portable layer for driving a u-connectXpress module with text commands instead of the
menu. It lives in `u-connectClientApp/src/ucxapp_*` and has no Windows dependencies, so the same
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
| `device.connect` | `[port]` | Connect to the module (default: saved port). Windows only |
| `device.disconnect` | | Disconnect. Windows only |
| `wifi.scan` | | List networks with BSSID, channel, RSSI, security |
| `wifi.connect` | `<ssid> [password]` | Connect to a network; waits up to 20 s for an IP. Omit the password for open networks |
| `wifi.status` | | Connection state, SSID, RSSI, IP |
| `wifi.disconnect` | | Leave the network |
| `sock.open` | `<tcp\|udp>` | Create a socket and print its handle |
| `sock.connect` | `<handle> <host> <port>` | Connect; waits up to 10 s |
| `sock.send` | `<handle> <text...>` | Send text (arguments are joined with single spaces, max 511 bytes) |
| `sock.read` | `<handle> [max] [wait_s]` | Wait for data (default 5 s) and print up to `max` bytes (default and maximum 512) |
| `sock.close` | `<handle>` | Close the socket |
| `sock.list` | | List open sockets |
| `net.ping` | `<host> [count]` | ICMP ping, count 1-10 (default 4); fails if no reply |
| `net.dns` | `<host>` | Resolve a host name |
| `http.get` | `<url> [show_bytes]` | GET over HTTP or HTTPS (scheme optional, default HTTP); prints the status line and up to `show_bytes` (default 512) of the body |
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
| `ucxapp_cmds_device.c`, `_wifi.c`, `_sock.c` | Command packs |

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

Steps 2 and 3 are done by the Windows app today in `u-connectClientApp.c`; moving them into the
portable layer is planned.
