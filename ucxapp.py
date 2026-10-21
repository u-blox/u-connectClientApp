#!/usr/bin/env python3
"""ucxapp, the u-connectXpressApp launcher: build, run, test and find hardware on Windows, Linux (also WSL)
and STM32.

  ucxapp                                    build if needed and start the app (this OS)
  ucxapp COM13 --exec "device.info"
  ucxapp --linux                            Linux build; on Windows it runs inside WSL
  ucxapp --stm32 --board f767 --build --flash --log
  ucxapp --detect                           find NORA modules, GNSS receivers and IMUs
  ucxapp --detect --test tests/test_hardware.py   hardware tests on what was found

Arguments the launcher does not know go to the app (to pytest with --test); everything after "--" too.
Windows: ucxapp.cmd calls this script. Linux: ./ucxapp.py or python3 ucxapp.py.
"""

import argparse
import concurrent.futures
import glob
import os
import platform
import re
import shutil
import signal
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BIN = ROOT / "app" / "bin"
SIGNED = ROOT / "app" / "release" / "ucxapp-signed.exe"
IS_WINDOWS = platform.system() == "Windows"
SUBMODULE_BRANCH = "cmag_matter_support"

BOARDS = {
    "h753": ("nucleo_h753zi", "NUCLEO-H753ZI", "H7", "STM32CubeH7", "stm32h7"),
    "f767": ("nucleo_f767zi", "NUCLEO-F767ZI", "F7", "STM32CubeF7", "stm32f7"),
    "f439": ("nucleo_f439zi", "NUCLEO-F439ZI", "F4", "STM32CubeF4", "stm32f4"),
}

UBLOX_VID = 0x1546
FTDI_VID = 0x0403
FTDI_PIDS = {0x6010, 0x6011, 0x6014, 0x601C, 0x6015}
UBLOX_GNSS_PIDS = {0x01A4, 0x01A5, 0x01A6, 0x01A7, 0x01A8, 0x01A9}
GNSS_DESC = re.compile(r"gnss|gps|\b(?:zed|neo|max|lea|evk)-[a-z]?\d", re.IGNORECASE)

# Old verbs of the former u-connectClientAppWin.cmd, kept so existing habits and docs still work.
LEGACY = {
    "debug": ["--debug"], "build": ["--build"], "clean": ["--clean"], "rebuild": ["--rebuild"], "all": ["--all"],
    "signed": ["--signed"], "hid": ["--hid"], "selftest": ["--selftest"], "update": ["--update"],
    "make-release": ["--release"], "sign": ["--sign"], "help": ["--help"],
}


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def require(tool, hint):
    if shutil.which(tool) is None:
        fail(f"{tool} not found. {hint}")


def check(cmd, what, **kw):
    print(">", " ".join(str(c) for c in cmd))
    if subprocess.call([str(c) for c in cmd], cwd=kw.pop("cwd", ROOT), **kw) != 0:
        fail(f"{what} failed")


def parse(argv):
    raw = []
    if "--" in argv:
        i = argv.index("--")
        argv, raw = argv[:i], argv[i + 1:]
    if argv and argv[0].lower() in LEGACY:
        head, argv = list(LEGACY[argv[0].lower()]), argv[1:]
        if head[0] in ("--clean", "--rebuild") and argv and argv[0].lower() in ("debug", "release"):
            head += ["--debug"] if argv[0].lower() == "debug" else []
            argv = argv[1:]
        argv = head + argv

    p = argparse.ArgumentParser(prog="ucxapp", description=__doc__, allow_abbrev=False,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    g = p.add_argument_group("target (default: this OS)")
    g.add_argument("--linux", action="store_true", help="Linux build; on Windows run it inside WSL")
    g.add_argument("--stm32", action="store_true", help="STM32 Nucleo firmware")
    g.add_argument("--board", default="h753", choices=[*BOARDS, "all"], help="STM32 board (default h753)")
    g = p.add_argument_group("build")
    g.add_argument("--build", action="store_true", help="build, then stop (add --run to start the app)")
    g.add_argument("--clean", action="store_true", help="remove build output")
    g.add_argument("--rebuild", action="store_true", help="clean, reconfigure and build")
    g.add_argument("--debug", action="store_true", help="Debug configuration (host builds)")
    g.add_argument("--all", action="store_true", help="build Debug and Release (Windows)")
    g.add_argument("--run", action="store_true", help="start the app after --build/--clean/--rebuild/--sign")
    g = p.add_argument_group("STM32")
    g.add_argument("--erase", action="store_true", help="erase the whole flash")
    g.add_argument("--recover", action="store_true", help="same as --erase (unlocks a stuck target)")
    g.add_argument("--flash", action="store_true", help="flash the built image over ST-Link")
    g.add_argument("--log", nargs="?", const="", metavar="PORT", help="console on the ST-Link VCP")
    g = p.add_argument_group("run")
    g.add_argument("--signed", action="store_true", help="run app/release/ucxapp-signed.exe")
    g.add_argument("--hid", action="store_true", help="start in HID keyboard mode")
    g = p.add_argument_group("tools")
    g.add_argument("--detect", action="store_true", help="find NORA modules, GNSS receivers and IMUs")
    g.add_argument("--test", action="store_true", help="run pytest (with --detect: on the detected hardware)")
    g.add_argument("--selftest", action="store_true", help="check the workspace and the built executables")
    g.add_argument("--update", action="store_true", help=f"update u-connectClient to {SUBMODULE_BRANCH}")
    g.add_argument("--sign", metavar="THUMBPRINT", help="sign the Release exe into app/release (Windows)")
    g.add_argument("--release", action="store_true", help="run scripts/release.py with the remaining arguments")
    args, extra = p.parse_known_args(argv)
    return args, extra + raw, argv


# ---- Host build (Windows / Linux) ----

def host_exe(debug):
    if not IS_WINDOWS:
        return BIN / "ucxapp"
    return BIN / ("ucxapp-debug.exe" if debug else "ucxapp.exe")


def newest_source():
    files = [*(ROOT / "app" / "src").iterdir(), ROOT / "app" / "CMakeLists.txt"]
    return max(f.stat().st_mtime for f in files if f.is_file())


def stale(exe):
    return not exe.exists() or newest_source() > exe.stat().st_mtime


def build_dir():
    return ROOT / "build" / ("windows" if IS_WINDOWS else "linux")


def host_clean(configs):
    bd = build_dir()
    if IS_WINDOWS:
        for c in configs:
            if (bd / "CMakeCache.txt").exists():
                subprocess.call(["cmake", "--build", str(bd), "--config", c, "--target", "clean"], cwd=ROOT)
    else:
        shutil.rmtree(bd, ignore_errors=True)
        (BIN / "ucxapp").unlink(missing_ok=True)
    print("Cleaned", bd.relative_to(ROOT))


def cache_moved(cache):
    """True when the CMake cache was made in another folder (repo moved or renamed)."""
    try:
        text = cache.read_text(errors="replace")
    except OSError:
        return False
    m = re.search(r"^CMAKE_CACHEFILE_DIR:INTERNAL=(.*)$", text, re.M)
    return m is not None and Path(m.group(1).strip()).resolve() != cache.parent.resolve()


def host_build(configs, fresh=False):
    require("cmake", "Install CMake 3.15+ from https://cmake.org/download/ and add it to PATH.")
    if not (ROOT / "u-connectClient" / "u-connectClient.cmake").exists():
        check(["git", "submodule", "update", "--init", "u-connectClient"], "submodule init")
    bd = build_dir()
    cache = bd / "CMakeCache.txt"
    if cache_moved(cache):
        print("Build folder was configured for another path; reconfiguring")
        fresh = True
        shutil.rmtree(bd / "CMakeFiles", ignore_errors=True)
    if fresh:
        cache.unlink(missing_ok=True)
    if IS_WINDOWS and not cache.exists():
        if subprocess.call(["cmake", "-S", "app", "-B", str(bd), "-A", "x64"], cwd=ROOT) != 0:
            fail("CMake configure failed. Visual Studio 2022 with 'Desktop development with C++' is needed: "
                 "https://aka.ms/vs/17/release/vs_BuildTools.exe")
    elif IS_WINDOWS and not (bd / "ucxapp.vcxproj").exists():
        # Build dir from before the exe was renamed; the cache keeps its platform.
        check(["cmake", "-S", "app", "-B", bd], "CMake reconfigure")
    for c in configs:
        if not IS_WINDOWS:
            check(["cmake", "-S", "app", "-B", bd, f"-DCMAKE_BUILD_TYPE={c}"], "CMake configure")
        check(["cmake", "--build", bd, "--config", c, "--target", "ucxapp", *(["--clean-first"] if fresh else [])],
              f"{c} build")


def pick_exe(args):
    """The exe to start: builds when sources are newer; prefers the signed exe when it is at least as new."""
    config = "Debug" if args.debug else "Release"
    exe = host_exe(args.debug)
    if args.signed:
        if not SIGNED.exists():
            fail(f"{SIGNED.relative_to(ROOT)} missing, create it with --sign THUMBPRINT")
        return SIGNED
    use_signed = IS_WINDOWS and not args.debug and SIGNED.exists()
    if use_signed and not exe.exists() and newest_source() <= SIGNED.stat().st_mtime + 60:
        return SIGNED
    if stale(exe):
        host_build([config])
    if use_signed and SIGNED.stat().st_mtime >= exe.stat().st_mtime:
        return SIGNED
    return exe


def wsl_serial_hint():
    try:
        if "microsoft" not in Path("/proc/version").read_text().lower():
            return
    except OSError:
        return
    if not glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"):
        print("NOTE: WSL sees no USB serial ports. Attach the EVK from Windows (admin PowerShell):\n"
              "  usbipd list; usbipd bind --busid <id>; usbipd attach --wsl --busid <id>")


def run_app(exe, app_args):
    print("Starting", exe.relative_to(ROOT), *app_args)
    if not IS_WINDOWS:
        wsl_serial_hint()
        os.chdir(exe.parent)
        os.execv(str(exe), [str(exe), *app_args])
    # The app handles Ctrl-C itself; the launcher must not die and kill it.
    old = signal.signal(signal.SIGINT, signal.SIG_IGN)
    try:
        return subprocess.call([str(exe), *app_args], cwd=exe.parent)
    finally:
        signal.signal(signal.SIGINT, old)


def sign(thumbprint):
    if not IS_WINDOWS:
        fail("--sign needs Windows (signtool and the certificate token)")
    sys.path.insert(0, str(ROOT / "scripts"))
    from release import find_signtool

    exe = host_exe(False)
    if not exe.exists():
        fail(f"{exe.relative_to(ROOT)} missing, build it first (--rebuild)")
    tool = find_signtool()
    check([tool, "sign", "/sha1", thumbprint, "/tr", "http://timestamp.digicert.com", "/td", "sha256",
           "/fd", "sha256", exe], "signing (is the certificate token inserted?)")
    check([tool, "verify", "/pa", exe], "signature verification")
    SIGNED.parent.mkdir(exist_ok=True)
    shutil.copy2(exe, SIGNED)
    print("Signed:", SIGNED.relative_to(ROOT))


def host(args, extra):
    configs = ["Debug", "Release"] if args.all else ["Debug" if args.debug else "Release"]
    if args.clean or args.rebuild:
        host_clean(configs if (args.debug or args.all or args.rebuild) else ["Debug", "Release"])
    if args.build or args.rebuild or args.all:
        host_build(configs, fresh=args.rebuild)
    if args.sign:
        sign(args.sign)
    if (args.build or args.clean or args.rebuild or args.all or args.sign) and not args.run:
        return 0
    exe = pick_exe(args)
    return run_app(exe, [*extra, "--exec", "hid.setup", "--shell"] if args.hid else extra)


# ---- Hardware detection ----

def classify(vid, pid, desc):
    d = desc.lower()
    if "stlink" in d or "st-link" in d:
        return "STM32 console"
    if "bluetooth" in d or "active management" in d:
        return "skip"
    if (vid == UBLOX_VID and pid in UBLOX_GNSS_PIDS) or GNSS_DESC.search(desc):
        return "GNSS"
    if vid == FTDI_VID and pid in FTDI_PIDS:
        return "EVK (FTDI)"
    if vid == UBLOX_VID:
        return "u-blox"
    if any(k in d for k in ("ch340", "cp210", "witmotion", "pl2303")):
        return "USB-UART (IMU/GNSS?)"
    return ""


def list_ports(exe):
    """(name, description, role) for every serial port; pyserial when installed, else the app's --list-ports."""
    try:
        from serial.tools import list_ports as lp

        return [(p.device, p.description or "", classify(p.vid, p.pid, p.description or "")) for p in lp.comports()]
    except ImportError:
        out = app_output(exe, "--list-ports", timeout=20)
        ports = []
        for line in out.splitlines():
            m = re.match(r"(\S+)\s+(?:available|no access)\s*(.*)", line)
            if m:
                ports.append((m.group(1), m.group(2), classify(None, None, m.group(2))))
        return ports


def app_output(exe, *app_args, timeout):
    try:
        r = subprocess.run([str(exe), *app_args], cwd=exe.parent, stdin=subprocess.DEVNULL, capture_output=True,
                           text=True, encoding="utf-8", errors="replace", timeout=timeout)
        return r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        print(f"  ({' '.join(app_args)} timed out after {timeout} s)")
        return ""


def module_ports(list_modules_output):
    """Port names from `--list-modules`; log lines such as '[..][DBG ] Loaded FTDI DLL' are ignored."""
    return re.findall(r"^\s*(COM\d+|/dev/\S+)\s*$", list_modules_output, re.MULTILINE)


def find_gnss(exe):
    """(port, baud, details) of the receiver gnss.detect reports first, or None."""
    out = app_output(Path(exe), "--exec", "gnss.detect", timeout=240)
    m = re.search(r"^\s*(COM\d+|/dev/[^\s:(]+)[^\n]*?GNSS at (\d+) baud:?\s*(.*)$", out, re.MULTILINE)
    return (m.group(1), m.group(2), m.group(3)) if m else None


def find_imu(exe):
    """(port, baud, kind) of the IMU imu.detect reports first, or None."""
    out = app_output(Path(exe), "--exec", "imu.detect", timeout=240)
    m = re.search(r"^\s*(COM\d+|/dev/[^\s:(]+)[^\n]*?:\s+(\S[^\n]*?) IMU at (\d+) baud", out, re.MULTILINE)
    return (m.group(1), m.group(3), m.group(2)) if m else None


def detect(exe):
    """Prints what is attached and returns the matching UCX_* test variables."""
    env = {}
    print("Serial ports:")
    for name, desc, role in list_ports(exe):
        print(f"  {name:<14} {role:<22} {desc}")

    modules = module_ports(app_output(exe, "--list-modules", timeout=30))
    print("NORA modules:")
    for i, port in enumerate(modules[:2]):
        env["UCX_PORT_" + "AB"[i]] = port
    with concurrent.futures.ThreadPoolExecutor() as pool:
        infos = list(pool.map(lambda p: app_output(exe, p, "--exec", "device.info", timeout=30), modules))
    for port, info in zip(modules, infos, strict=True):
        m = re.search(r"\b(NORA-[A-Z0-9]+[\w-]*)", info)
        print(f"  {port:<14} {m.group(1) if m else '(no answer to device.info)'}")
    if not modules:
        print("  none")

    print("GNSS (gnss.detect):")
    gnss = find_gnss(exe)
    if gnss:
        env["UCX_GNSS_PORT"], env["UCX_GNSS_BAUD"] = gnss[0], gnss[1]
        print(f"  {gnss[0]} at {gnss[1]} baud {gnss[2]}")
    else:
        print("  none")

    print("IMU (imu.detect):")
    imu = find_imu(exe)
    if imu:
        env["UCX_IMU_PORT"], env["UCX_IMU_BAUD"] = imu[0], imu[1]
        print(f"  {imu[0]} {imu[2]} at {imu[1]} baud")
    else:
        print("  none")

    if env:
        print("Test variables:")
        for k, v in env.items():
            print(f'  $env:{k} = "{v}"' if IS_WINDOWS else f"  export {k}={v}")
    return env


def run_tests(pytest_args, found):
    if subprocess.call([sys.executable, "-c", "import pytest"], stderr=subprocess.DEVNULL) != 0:
        fail(f"pytest missing for {sys.executable}: {sys.executable} -m pip install -r tests/requirements.txt")
    env = dict(os.environ)
    for k, v in found.items():
        env.setdefault(k, v)
    return subprocess.call([sys.executable, "-m", "pytest", *(pytest_args or ["tests"])], cwd=ROOT, env=env)


# ---- STM32 ----

def stm32_tools():
    if IS_WINDOWS:
        extra = [Path(r"C:\Program Files\CMake\bin"),
                 Path(r"C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin"),
                 ROOT.parent / "u-connectMatter" / "tools"]
        os.environ["PATH"] = os.pathsep.join([*(str(d) for d in extra if d.is_dir()), os.environ["PATH"]])
    for _, _, family, hal_dir, portdir in BOARDS.values():
        var = f"STM32CUBE{family}_PATH"
        if os.environ.get(var):
            continue
        candidates = [ROOT / "u-connectClient" / "ports" / "extra" / portdir / hal_dir,
                      ROOT.parent / "u-connectMatter" / "u-connectClient" / "ports" / "extra" / portdir / hal_dir]
        hal = next((c for c in candidates if (c / "Drivers").is_dir()), None)
        if hal is not None:
            os.environ[var] = str(hal)


def programmer():
    win = Path(r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe")
    found = shutil.which("STM32_Programmer_CLI") or (str(win) if win.exists() else None)
    if found is None:
        fail("STM32CubeProgrammer not found (STM32_Programmer_CLI)")
    return found


def find_stlink(prog, board_name):
    """ST-Link serial number of the probe whose board name matches; the list has an SN line before each name."""
    out = subprocess.run([prog, "-l", "st"], capture_output=True, text=True, errors="replace").stdout
    last = None
    for line in out.splitlines():
        key, _, value = line.partition(":")
        value = value.replace(" ", "")
        if "ST-LINK SN" in key:
            last = value
        elif "Board Name" in key and value == board_name:
            return last
    return None


def stm32_build(board, family, hal_dir, portdir, bd):
    for tool in ("cmake", "ninja", "arm-none-eabi-gcc"):
        require(tool, "See .github/skills/ucx-stm32/SKILL.md for the STM32 toolchain.")
    hal = os.environ.get(f"STM32CUBE{family}_PATH")
    if hal is None:
        fail(f"{hal_dir} not found. Set STM32CUBE{family}_PATH or run: "
             f"git -C u-connectClient submodule update --init ports/extra/{portdir}/{hal_dir}")
    if not (bd / "build.ninja").exists():
        toolchain = ROOT / "u-connectClient" / "cmake" / "arm-none-eabi-gcc.cmake"
        check(["cmake", "-G", "Ninja", "-S", ROOT / "app" / "stm32", "-B", bd,
               f"-DCMAKE_TOOLCHAIN_FILE={toolchain.as_posix()}", f"-DSTM32_BOARD={board}",
               f"-DSTM32_HAL_PATH={Path(hal).as_posix()}", "-DCMAKE_BUILD_TYPE=Release"], "CMake configure")
    check(["cmake", "--build", bd], "STM32 build")


def stm32_log(port):
    if IS_WINDOWS:
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
               str(ROOT / "app" / "stm32" / "stm32tool.ps1"), "-Action", "log", *(["-Port", port] if port else [])]
        return subprocess.call(cmd, cwd=ROOT)
    port = port or next(iter(sorted(glob.glob("/dev/serial/by-id/*STLINK*") + glob.glob("/dev/serial/by-id/*STM*"))),
                        None)
    if port is None:
        fail("no ST-Link VCP found, pass it: --log /dev/ttyACM0")
    return subprocess.call([sys.executable, "-m", "serial.tools.miniterm", port, "115200"])


def stm32_all(args):
    if args.erase or args.recover or args.log is not None:
        fail("--board all supports only --clean, --build, --rebuild and --flash")
    flags = [f for f, on in (("--clean", args.clean), ("--build", args.build), ("--rebuild", args.rebuild),
                             ("--flash", args.flash)) if on]
    if not flags:
        fail("--board all needs --build, --clean, --rebuild or --flash")
    (ROOT / "build").mkdir(exist_ok=True)
    procs = {}
    for b in BOARDS:
        log = open(ROOT / "build" / f"all_{b}.log", "w", encoding="utf-8")
        procs[b] = (subprocess.Popen([sys.executable, __file__, "--stm32", "--board", b, *flags], cwd=ROOT,
                                     stdout=log, stderr=subprocess.STDOUT), log)
    failed = False
    for b, (proc, log) in procs.items():
        rc = proc.wait()
        log.close()
        print(f"[{b}] ok" if rc == 0 else f"[{b}] FAILED, see build/all_{b}.log")
        failed |= rc != 0
    return 1 if failed else 0


def stm32(args, extra):
    if extra:
        fail(f"unknown STM32 argument(s): {' '.join(extra)}")
    if not (args.clean or args.build or args.rebuild or args.erase or args.recover or args.flash
            or args.log is not None):
        fail("--stm32 needs --build, --rebuild, --clean, --erase, --recover, --flash or --log")
    stm32_tools()
    if args.board == "all":
        return stm32_all(args)
    board, name, family, hal_dir, portdir = BOARDS[args.board]
    bd = ROOT / "build" / f"stm32_{args.board}"
    image = ROOT / "build" / "bin" / f"ucxapp_{board}.bin"
    if args.clean or args.rebuild:
        shutil.rmtree(bd, ignore_errors=True)
        for f in image.parent.glob(f"ucxapp_{board}.*"):
            f.unlink()
        print("Cleaned", bd.relative_to(ROOT))
    if args.build or args.rebuild:
        stm32_build(board, family, hal_dir, portdir, bd)
        print("Built", image.relative_to(ROOT))
    if args.erase or args.recover:
        check([programmer(), "-c", "port=SWD", "mode=UR", "reset=HWrst", "-e", "all"], "erase")
    if args.flash:
        if not image.exists():
            fail(f"{image.relative_to(ROOT)} missing, add --build")
        prog = programmer()
        sn = find_stlink(prog, name)
        if sn is None:
            fail(f"no ST-Link found for {name}")
        print(f"Flashing {name} via ST-Link {sn}")
        check([prog, "-c", "port=SWD", f"sn={sn}", "mode=UR", "reset=HWrst", "-d", image, "0x08000000", "-v",
               "-rst"], "flash")
    if args.log is not None:
        return stm32_log(args.log)
    return 0


# ---- Workspace tools ----

def update():
    require("git", "Install Git from https://git-scm.com/downloads")
    sub = ROOT / "u-connectClient"
    if not (sub / ".git").exists():
        check(["git", "submodule", "update", "--init", "u-connectClient"], "submodule init")
    else:
        check(["git", "-C", sub, "fetch", "origin"], "fetch")
        check(["git", "-C", sub, "checkout", SUBMODULE_BRANCH], "checkout")
        check(["git", "-C", sub, "pull", "origin", SUBMODULE_BRANCH], "pull")
    subprocess.call(["git", "submodule", "status"], cwd=ROOT)
    print("Commit the new submodule reference with: git add u-connectClient; git commit")
    return 0


def selftest():
    errors = 0
    for rel in ("u-connectClient/src", "u-connectClient/inc", "u-connectClient/ucx_api/generated",
                "u-connectClient/src/u_cx_at_client.c", "app/src/ucxapp_main.c", "app/CMakeLists.txt",
                "app/stm32/CMakeLists.txt", "README.md"):
        ok = (ROOT / rel).exists()
        errors += not ok
        print(f"[{'OK' if ok else 'ERROR'}] {rel}")
    headers = len(list((ROOT / "u-connectClient" / "ucx_api" / "generated").rglob("u_cx_*.h")))
    print(f"[{'OK' if headers > 10 else 'WARNING'}] {headers} generated UCX API headers")
    for exe in (host_exe(False), host_exe(True), SIGNED):
        if exe.exists():
            out = app_output(exe, "-v", timeout=10).strip()
            errors += not out
            print(f"[{'OK' if out else 'ERROR'}] {exe.relative_to(ROOT)}: {out or 'does not run'}")
        else:
            print(f"[INFO] {exe.relative_to(ROOT)} not built")
    stm32_tools()
    for tool in ("cmake", "git", "ninja", "arm-none-eabi-gcc"):
        print(f"[{'OK' if shutil.which(tool) else 'INFO'}] {tool} {'found' if shutil.which(tool) else 'not on PATH'}")
    for _, _, family, _, _ in BOARDS.values():
        var = f"STM32CUBE{family}_PATH"
        print(f"[{'OK' if os.environ.get(var) else 'INFO'}] {var} {os.environ.get(var) or 'not set'}")
    print("Selftest passed" if errors == 0 else f"Selftest: {errors} error(s)")
    return 1 if errors else 0


def main(argv):
    args, extra, parsed_argv = parse(argv)
    if args.release:
        return subprocess.call([sys.executable, str(ROOT / "scripts" / "release.py"), *extra], cwd=ROOT)
    if args.update:
        return update()
    if args.selftest:
        return selftest()
    if args.linux and IS_WINDOWS:
        require("wsl", "Install WSL with: wsl --install")
        forward = [a for a in parsed_argv if a != "--linux"]
        if "--" in argv:
            forward += argv[argv.index("--"):]
        return subprocess.call(["wsl", "--cd", str(ROOT), "--", "python3", "ucxapp.py", *forward])
    if args.stm32:
        return stm32(args, extra)
    found = {}
    if args.detect or args.test:
        # Detection and tests use the development exe, never an older signed one.
        exe = host_exe(args.debug)
        if stale(exe):
            host_build(["Debug" if args.debug else "Release"])
        if args.detect:
            found = detect(exe)
            if not args.test:
                return 0 if found else 1
            # Empty = detected as absent, so the tests do not probe the ports again.
            for key in ("UCX_PORT_A", "UCX_PORT_B", "UCX_GNSS_PORT", "UCX_IMU_PORT"):
                found.setdefault(key, "")
        return run_tests(extra, {**found, "UCX_APP_EXE": str(exe)})
    return host(args, extra)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
