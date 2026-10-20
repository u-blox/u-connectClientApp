#!/usr/bin/env python3
"""Release u-connectClientApp: tag vX.Y.Z.YYMMDD, let CI build Linux and STM32,
sign and upload the Windows exe.

1. Checks the work tree is clean, on master and in sync with origin.
2. Windows only: builds the Windows Release exe and signs it (the EV certificate is only available locally).
3. Tags and pushes; .github/workflows/release.yml builds Linux + STM32 and creates a draft release.
4. Waits for the draft, uploads the signed exe and its SHA-256 (Windows only), optionally publishes.

X.Y.Z is APP_VERSION_MAJOR/MINOR/PATCH in app/CMakeLists.txt, YYMMDD is today.

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
EXPECTED_ASSETS = 10  # Linux + 3 STM32 boards
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
    cmake = (ROOT / "app" / "CMakeLists.txt").read_text(encoding="utf-8")
    parts = []
    for n in ("MAJOR", "MINOR", "PATCH"):
        m = re.search(rf"set\(APP_VERSION_{n}\s+(\d+)\)", cmake)
        if not m:
            fail(f"APP_VERSION_{n} not found in app/CMakeLists.txt")
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
    run("cmake", "--build", "build/windows", "--config", "Release", "--target", "u-connectClientApp", "--clean-first")
    built = ROOT / "app" / "bin" / "u-connectClientApp.exe"
    if not built.exists():
        fail(f"{built} not found")

    step("Signing")
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    exe = dist / "u-connectClientApp-windows-x64.exe"
    shutil.copy2(built, exe)
    run(signtool, "sign", "/sha1", thumbprint, "/tr", "http://timestamp.digicert.com",
        "/td", "sha256", "/fd", "sha256", exe)
    run(signtool, "verify", "/pa", exe)
    digest = hashlib.sha256(exe.read_bytes()).hexdigest()
    Path(f"{exe}.sha256").write_text(f"{digest}  {exe.name}\n", encoding="ascii")
    print(f"SHA-256: {digest}")
    return exe


def wait_for_draft(tag, minutes):
    step(f"Waiting for CI to create the draft release (Linux + 3 STM32 boards = {EXPECTED_ASSETS} files)")
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
    run("git", "tag", "-a", tag, "-m", f"Release u-connectClientApp {tag}")
    run("git", "push", "origin", tag)

    if not wait_for_draft(tag, args.wait_minutes):
        upload = f' gh release upload {tag} "{exe}" "{exe}.sha256"' if exe else ""
        fail(f"CI did not finish in {args.wait_minutes} min; check the Actions tab, then run:{upload}")

    if exe:
        step("Uploading signed Windows exe")
        run("gh", "release", "upload", tag, exe, f"{exe}.sha256", "--clobber")

    if args.publish:
        step("Publishing")
        run("gh", "release", "edit", tag, "--draft=false")
        print(f"Published: {out('gh', 'release', 'view', tag, '--json', 'url', '--jq', '.url')[1]}")
    else:
        print(f"\nDraft ready. Review it and publish: gh release edit {tag} --draft=false")


if __name__ == "__main__":
    main()
