#!/usr/bin/env bash
# Build and launch u-connectClientApp (Linux/macOS counterpart of u-connectClientApp.cmd).
# Usage: ./u-connectClientApp.sh [build|debug|clean|rebuild|update|help] [-- app args]
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR=build
APP_DIR=u-connectClientApp
CONFIG=Release
ACTION=run

usage() {
    cat <<EOF
u-connectClientApp launcher

  ./u-connectClientApp.sh               Build if needed and run
  ./u-connectClientApp.sh debug         Debug build and run
  ./u-connectClientApp.sh build         Build only
  ./u-connectClientApp.sh clean         Remove build artifacts
  ./u-connectClientApp.sh rebuild       Clean, then build
  ./u-connectClientApp.sh update        Update the u-connectClient submodule

Arguments after the action are passed to the app, e.g.
  ./u-connectClientApp.sh /dev/ttyUSB0 --exec "device.info"

Note: the app is still Windows-only (FTDI, Win32 serial). The Linux port of the
portable command layer (src/ucxapp_*) is in progress; until then CMake stops
with an explanatory message on non-Windows hosts.
EOF
}

case "${1:-}" in
    help|--help|-h) usage; exit 0 ;;
    debug)   CONFIG=Debug; shift ;;
    build)   ACTION=build; shift ;;
    clean)   ACTION=clean; shift ;;
    rebuild) ACTION=rebuild; shift ;;
    update)  ACTION=update; shift ;;
esac

clean() { rm -rf "${BUILD_DIR:?}" "${APP_DIR:?}/bin"; echo "Cleaned."; }

build() {
    command -v cmake >/dev/null || { echo "ERROR: cmake not found" >&2; exit 1; }
    [ -f u-connectClient/u-connectClient.cmake ] || git submodule update --init u-connectClient
    cmake -S "$APP_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"
    cmake --build "$BUILD_DIR" --config "$CONFIG"
}

case "$ACTION" in
    update)  git submodule update --remote u-connectClient; exit 0 ;;
    clean)   clean; exit 0 ;;
    rebuild) clean; build; exit 0 ;;
    build)   build; exit 0 ;;
esac

EXE="$APP_DIR/bin/u-connectClientApp"
[ "$CONFIG" = Debug ] && EXE="${EXE}-debug"
[ -x "$EXE" ] || build
exec "$EXE" "$@"
