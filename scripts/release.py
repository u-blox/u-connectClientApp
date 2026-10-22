#!/usr/bin/env python3
"""Release u-connectXpressApp: tag vX.Y.Z.YYMMDD, let CI build Linux and STM32,
sign and upload the Windows exe.

1. Checks the work tree is clean, on master and in sync with origin.
2. Windows only: builds the Windows Release exe and signs it (the EV certificate is only available locally).
3. Tags and pushes; .github/workflows/release.yml builds Linux + STM32 and creates a draft release.
4. Waits for the draft, uploads the signed exe and its SHA-256 (Windows only), optionally publishes.

X.Y.Z is U_CX_VERSION_MAJOR/MINOR/PATCH in u_cx_version.h (the same header app/CMakeLists.txt reads), YYMMDD is today.

Examples:
    python scripts/release.py --dry-run     # build and sign only, nothing is tagged or pushed
    python scripts/release.py --publish     # full release, publish the draft when done
"""
import argparse
import datetime
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_THUMBPRINT = "EF3FD135F1CD669E0D7F4F2CF14FE1334EECD16E"
STM32_BOARDS = ("nucleo_h753zi", "nucleo_f767zi", "nucleo_f439zi", "f407_disc", "nucleo_f401re",
                "nucleo_f446re", "bluepill_f103c8")
EXPECTED_ASSETS = 1 + len(STM32_BOARDS)  # one archive for Linux and one per STM32 board
IS_WINDOWS = platform.system() == "Windows"


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def step(msg):
    print(f"\n== {msg}")


def run(*cmd, check=True):
    result = subprocess.run([str(c) for c in cmd], cwd=ROOT, text=True, capture_output=False)
    if check and result.returncode != 0:
        fail(f"{' '.join(str(c) for c in cmd)} failed ({result.returncode})")


def out(*cmd):
    result = subprocess.run([str(c) for c in cmd], cwd=ROOT, text=True, capture_output=True)
    return result.returncode, result.stdout.strip()


def read_version():
    header_path = ROOT / "u-connectClient" / "ucx_api" / "generated" / "NORA-W36X" / "u_cx_version.h"
    header = header_path.read_text(encoding="utf-8")
    parts = []
    for n in ("MAJOR", "MINOR", "PATCH"):
        m = re.search(rf"#define U_CX_VERSION_{n} (\d+)", header)
        if not m:
            fail(f"U_CX_VERSION_{n} not found in u_cx_version.h")
        parts.append(m.group(1))
    return "v{}.{}.{}.{}".format(*parts, datetime.date.today().strftime("%y%m%d"))


def find_signtool():
    base = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Windows Kits" / "10" / "bin"
    found = sorted((p for p in base.rglob("signtool.exe") if "x64" in p.parts), reverse=True)
    if not found:
        fail("signtool.exe not found (install the Windows SDK)")
    return found[0]


def build_and_sign(thumbprint):
    signtool = find_signtool()
    step("Building Windows Release")
    run("cmake", "-S", "app", "-B", "build/windows", "-A", "x64")
    run("cmake", "--build", "build/windows", "--config", "Release", "--target", "ucxapp", "--clean-first")
    built = ROOT / "app" / "bin" / "ucxapp.exe"
    if not built.exists():
        fail(f"{built} not found")

    step("Signing")
    dist = ROOT / "dist"
    folder = dist / "ucxapp-windows-x64"
    shutil.rmtree(folder, ignore_errors=True)
    folder.mkdir(parents=True)
    exe = folder / "ucxapp.exe"
    shutil.copy2(built, exe)
    run(signtool, "sign", "/sha1", thumbprint, "/tr", "http://timestamp.digicert.com",
        "/td", "sha256", "/fd", "sha256", exe)
    run(signtool, "verify", "/pa", exe)
    digest = hashlib.sha256(exe.read_bytes()).hexdigest()
    Path(f"{exe}.sha256").write_text(f"{digest}  {exe.name}\n", encoding="ascii")
    print(f"SHA-256: {digest}")
    archive = shutil.make_archive(str(folder), "zip", root_dir=dist, base_dir=folder.name)
    print(f"Packaged: {archive}")
    return Path(archive)


def wait_for_draft(tag, minutes):
    step(f"Waiting for CI to create the draft release ({EXPECTED_ASSETS} archives: Linux and the STM32 boards)")
    deadline = time.time() + minutes * 60
    while time.time() < deadline:
        time.sleep(20)
        code, text = out("gh", "release", "view", tag, "--json", "isDraft,assets")
        if code == 0 and text:
            rel = json.loads(text)
            count = len(rel["assets"])
            print(f"  draft={rel['isDraft']} assets={count}")
            if count >= EXPECTED_ASSETS:
                return True
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--thumbprint", default=DEFAULT_THUMBPRINT)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--publish", action="store_true")
    ap.add_argument("--wait-minutes", type=int, default=30)
    args = ap.parse_args()

    tag = read_version()
    print(f"Release tag: {tag}")

    step("Checking repository")
    if not shutil.which("gh"):
        fail("GitHub CLI (gh) not found")
    if IS_WINDOWS and not shutil.which("cmake"):
        fail("cmake not found")
    if not args.dry_run:
        run("gh", "auth", "status")
        if out("git", "status", "--porcelain")[1]:
            fail("Work tree is not clean; commit or stash first")
        if out("git", "branch", "--show-current")[1] != "master":
            fail("Not on master")
        run("git", "fetch", "origin", "master", "--tags")
        if out("git", "rev-parse", "HEAD")[1] != out("git", "rev-parse", "origin/master")[1]:
            fail("master differs from origin/master; push or pull first")
        if out("git", "tag", "-l", tag)[1]:
            fail(f"Tag {tag} already exists")

    exe = None
    if IS_WINDOWS:
        exe = build_and_sign(args.thumbprint)
    else:
        print("Not on Windows: skipping Windows build and signing; the signed exe must be uploaded separately.")

    if args.dry_run:
        print(f"\nDry run done{': ' + str(exe) if exe else ''}")
        return

    step(f"Tagging and pushing {tag}")
    run("git", "tag", "-a", tag, "-m", f"Release u-connectXpressApp {tag}")
    run("git", "push", "origin", tag)

    if not wait_for_draft(tag, args.wait_minutes):
        upload = f' gh release upload {tag} "{exe}"' if exe else ""
        fail(f"CI did not finish in {args.wait_minutes} min; check the Actions tab, then run:{upload}")

    if exe:
        step("Uploading signed Windows zip")
        run("gh", "release", "upload", tag, exe, "--clobber")

    if args.publish:
        step("Publishing")
        run("gh", "release", "edit", tag, "--draft=false")
        print(f"Published: {out('gh', 'release', 'view', tag, '--json', 'url', '--jq', '.url')[1]}")
    else:
        print(f"\nDraft ready. Review it and publish: gh release edit {tag} --draft=false")


if __name__ == "__main__":
    main()
