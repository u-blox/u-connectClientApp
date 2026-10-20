#!/usr/bin/env bash
# Build and launch u-connectClientApp (Linux/macOS counterpart of u-connectClientAppWin.cmd).
# Usage: ./u-connectClientAppLinux.sh [build|debug|clean|rebuild|update|help] [-- app args]
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR=build/linux
APP_DIR=u-connectClientApp
CONFIG=Release
ACTION=run

usage() {
    cat <<EOF
u-connectClientApp launcher

  ./u-connectClientAppLinux.sh               Build if needed and run
  ./u-connectClientAppLinux.sh debug         Debug build and run
  ./u-connectClientAppLinux.sh build         Build only
  ./u-connectClientAppLinux.sh clean         Remove build artifacts
  ./u-connectClientAppLinux.sh rebuild       Clean, then build
  ./u-connectClientAppLinux.sh update        Update the u-connectClient submodule

Arguments after the action are passed to the app, e.g.
  ./u-connectClientAppLinux.sh /dev/ttyUSB0 --exec "device.info"

Note: the Linux build is the portable command layer (src/ucxapp_*), binary
u-connectClientApp/bin/ucxapp. Build files go to build/linux so they do not
clash with the Windows and STM32 build directories.
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

clean() { rm -rf "${BUILD_DIR:?}" "${APP_DIR:?}/bin/ucxapp"; echo "Cleaned."; }

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

EXE="$APP_DIR/bin/ucxapp"
[ -x "$EXE" ] || build
exec "$EXE" "$@"
