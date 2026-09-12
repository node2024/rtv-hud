#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
# Stop the managed server before allocating compiler memory. Fail closed if
# systemd cannot be reached, or another CS2 process is still running.
systemctl stop cs2server.service
state=$(systemctl show cs2server.service --property=ActiveState --value)
[[ "$state" == inactive ]] || { echo "CS2 is not stopped: $state" >&2; exit 1; }
if pgrep -x cs2 >/dev/null; then
  echo 'A CS2 process is still running; refusing to build.' >&2
  exit 1
fi
cmake -S . -B build-khook -DCMAKE_BUILD_TYPE=Release \
  -DSDK="${RTVHUD_SDK:-$PWD/deps/reference/hl2sdk-cs2}" \
  -DMMS="${RTVHUD_MMS:-$PWD/deps/metamod-source}"
cmake --build build-khook -j1
ctest --test-dir build-khook --output-on-failure
