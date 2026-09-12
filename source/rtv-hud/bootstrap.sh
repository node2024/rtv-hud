#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if [[ ! -d deps/reference/.git ]]; then
  git clone --depth 1 --branch v0.0.167 https://github.com/KZGlobalTeam/cs2kz-metamod.git deps/reference
fi
test "$(git -C deps/reference rev-parse HEAD)" = bde8b06b30d459bf280cee250e9d92417f4201ce
git -C deps/reference submodule update --init --depth 1 hl2sdk-cs2
if [[ ! -d deps/metamod-source/.git ]]; then
  git clone https://github.com/alliedmodders/metamod-source.git deps/metamod-source
fi
git -C deps/metamod-source checkout --detach 7e24ce9
git -C deps/metamod-source submodule update --init --depth 1 third_party/khook
test -f deps/metamod-source/third_party/khook/include/khook.hpp
printf '%s\n' 'SDK: deps/reference/hl2sdk-cs2; MMS: deps/metamod-source (KHook / API 18)'
