"""Drive ucx-windows-app from Python through a Windows pseudo console (ConPTY).

The app polls the console with _kbhit(), so piped stdin does not work; a ConPTY gives it a
real console while the script reads the screen text and types lines like a user.
"""

from __future__ import annotations

import os
import re
import subprocess
import threading
import time
from pathlib import Path

ANSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]|\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)|\x1b[()][A-B0-2]|\x1b[=>]")


class ExpectTimeout(AssertionError):
    pass


class UcxApp:
    """One running instance of the app. Use as a context manager."""

    def __init__(self, exe, port=None, ini=None, name="app", log_dir=None, cols=200, rows=50):
        self.exe = str(exe)
        self.port = port
        self.ini = str(ini) if ini else None
        self.name = name
        self.cols, self.rows = cols, rows
        self._log_path = Path(log_dir) / f"{name}.log" if log_dir else None
        self._buf = ""
        self._pos = 0
        self._lock = threading.Lock()
        self._proc = None
        self._reader = None

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *exc):
        self.close()

    # -- lifecycle -----------------------------------------------------------------------------

    def start(self):
        from winpty import PtyProcess  # imported lazily so non-Windows collection still works

        argv = [self.exe]
        if self.port:
            argv.append(self.port)
        if self.ini:
            argv += ["--ini", self.ini]
        if self._log_path:
            self._log_path.parent.mkdir(parents=True, exist_ok=True)
            self._log_path.write_text("", encoding="utf-8")
        self._proc = PtyProcess.spawn(argv, cwd=os.path.dirname(self.exe), dimensions=(self.rows, self.cols))
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self):
        while True:
            try:
                data = self._proc.read(4096)
            except EOFError:
                return
            except Exception:  # noqa: BLE001 - reader must never raise into the interpreter
                return
            if not data:
                if not self._proc.isalive():
                    return
                time.sleep(0.01)
                continue
            text = ANSI_RE.sub("", data).replace("\r", "")
            with self._lock:
                self._buf += text
            if self._log_path:
                with self._log_path.open("a", encoding="utf-8") as f:
                    f.write(text)

    def close(self):
        if self._proc is not None and self._proc.isalive():
            try:
                self._proc.terminate(force=True)
            except OSError:
                # winpty's os.kill is denied for pty children; taskkill works and frees the COM port.
                subprocess.run(["taskkill", "/F", "/T", "/PID", str(self._proc.pid)], capture_output=True, check=False)
        self._proc = None

    @property
    def alive(self):
        return self._proc is not None and self._proc.isalive()

    @property
    def exit_code(self):
        return None if self.alive else getattr(self._proc, "exitstatus", None)

    # -- screen text ---------------------------------------------------------------------------

    @property
    def output(self):
        with self._lock:
            return self._buf

    def tail(self, n=1500):
        return self.output[-n:]

    def expect(self, pattern, timeout=10.0):
        """Wait for regex `pattern` in output not yet consumed; consume up to the end of the match."""
        rx = re.compile(pattern) if isinstance(pattern, str) else pattern
        deadline = time.monotonic() + timeout
        while True:
            with self._lock:
                m = rx.search(self._buf, self._pos)
                if m:
                    self._pos = m.end()
                    return m
            if time.monotonic() >= deadline:
                raise ExpectTimeout(f"[{self.name}] timeout waiting for {rx.pattern!r}; last output:\n{self.tail()}")
            time.sleep(0.05)

    def expect_any(self, patterns, timeout=10.0):
        """Return (index, match) of whichever pattern appears first."""
        rxs = [re.compile(p) for p in patterns]
        deadline = time.monotonic() + timeout
        while True:
            with self._lock:
                hits = [(m.start(), i, m) for i, rx in enumerate(rxs) if (m := rx.search(self._buf, self._pos))]
                if hits:
                    _, i, m = min(hits, key=lambda h: h[0])
                    self._pos = m.end()
                    return i, m
            if time.monotonic() >= deadline:
                raise ExpectTimeout(
                    f"[{self.name}] timeout waiting for any of {patterns!r}; last output:\n{self.tail()}"
                )
            time.sleep(0.05)

    def skip_output(self):
        """Discard everything received so far so the next expect() only sees new output."""
        with self._lock:
            self._pos = len(self._buf)

    # -- input ---------------------------------------------------------------------------------

    def send(self, line=""):
        self._proc.write(line + "\r")

    def acknowledge(self, timeout=10.0):
        """Answer a 'Press Enter to continue' prompt."""
        self.expect(r"Press Enter to continue", timeout)
        self.send("")

    # -- app level helpers ---------------------------------------------------------------------

    def wait_main_menu(self, timeout=30.0):
        return self.expect(r"u-blox ucxclient Windows App", timeout)

    def wait_connected(self, timeout=30.0):
        """Wait until the main menu reports an active connection; returns the device model line."""
        self.expect(r"CONNECTION:\s+\S+ ACTIVE", timeout)
        return self.expect(r"DEVICE:\s+(\S+)", 10.0).group(1)

    def menu(self, key, title, timeout=15.0):
        """Type a menu key and wait for the sub-menu title."""
        self.send(key)
        return self.expect(title, timeout)

    def back_to_main(self):
        self.send("0")
        self.wait_main_menu(10.0)

    def return_to_menu(self, title, timeout=30.0, answers=None):
        """Finish an action: answer 'Press Enter' and y/n prompts until the menu `title` is redrawn."""
        answers = {r"\((?:y/N|Y/n|y/n)\):": "n", **(answers or {})}
        replies = list(answers.values())
        patterns = [title, r"Press Enter to continue", *answers.keys()]
        deadline = time.monotonic() + timeout
        while True:
            i, _ = self.expect_any(patterns, max(0.1, deadline - time.monotonic()))
            if i == 0:
                return
            self.send("" if i == 1 else replies[i - 2])

    def quit(self, timeout=10.0):
        self.send("q")
        deadline = time.monotonic() + timeout
        while self.alive and time.monotonic() < deadline:
            time.sleep(0.1)
        return not self.alive
