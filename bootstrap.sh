#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
sdk_revision=d4a8b46676ded25b9d956287ec40ef3c9e12c14c
mms_revision=170058a2f0dbd58106b7c76bfe88a94672295356
if [[ ! -d deps/s2sdk/.git ]]; then
  git clone --depth 1 --branch cs2 https://github.com/alliedmodders/s2sdk.git deps/s2sdk
fi
git -C deps/s2sdk fetch --depth 1 origin "$sdk_revision"
git -C deps/s2sdk checkout --detach "$sdk_revision"
if [[ ! -d deps/metamod-source/.git ]]; then
  git clone https://github.com/alliedmodders/metamod-source.git deps/metamod-source
fi
git -C deps/metamod-source fetch --depth 1 origin "$mms_revision"
git -C deps/metamod-source checkout --detach "$mms_revision"
git -C deps/metamod-source submodule update --init --depth 1 third_party/khook
test -f deps/metamod-source/third_party/khook/include/khook.hpp
printf '%s\n' 'SDK: deps/s2sdk (2026-10-23); MMS: deps/metamod-source (KHook / API 18)'
