# RTV HUD 0.9.0

English | [日本語](README-ja.md)

RTV HUD is a map browser and map voting HUD plugin for Counter-Strike 2 servers
running Linux x86_64. Version 0.9.0 migrates five hooks from SourceHook to KHook.

Server installation and plugin loading were verified on September 12, 2026,
and a user also confirmed successful in-game operation. The bundled binary has
the same SHA-256 hash as the verified running binary; it was not rebuilt for
this release package.

## Development approach

This project is developed primarily through **Vibe Coding**: an iterative,
AI-assisted development process using natural-language prompts to create and
refine code.

## Requirements

- A CS2 dedicated server running Linux x86_64.
- A KHook-compatible version of Metamod:Source (plugin API 18).
  Verified version: **2.0.0-dev+1467**.
- A KHook-compatible version of MultiAddonManager. Verified version: **1.6**.
- HUD Workshop addon **3797394226**.

The plugin cannot load with older SourceHook / API 17 versions of Metamod.
When upgrading Metamod, update any other installed plugins to KHook-compatible
versions as well.

The verified server setup used CS2KZ 0.0.172 and SQLMM 1.3.4.3, with
CounterStrikeSharp and ClientCvarValue disabled. RTV HUD itself does not depend
on CS2KZ, SQLMM, CounterStrikeSharp, or Swiftly.

## Package contents

- `game/csgo/`: Linux plugin binary, VDF file, configuration, and a list of 364 maps.
- `source/rtv-hud/`: corresponding source code, tests, build scripts, and Workshop assets.
- `LICENSE`, `NOTICE.md`, and `THIRD_PARTY/`: license and attribution information.
- `release-manifest.json` and `SHA256SUMS.txt`: release metadata and file checksums.

The bundled `admins.txt` is intentionally empty, including the copy in the
source tree's `deploy` directory. The package contains no server credentials,
administrator IDs, databases, logs, or binaries for other plugins.
The Workshop VPK and map files are distributed separately.

`source/rtv-hud/bridge` contains legacy integration source code and is not used
to install version 0.9.0.

## Installation and upgrading from 0.8.0

1. Stop the CS2 server and back up your existing plugin binary and configuration.
2. Ensure that Metamod and your other installed plugins are KHook-compatible.
3. Copy the contents of this package's `game/csgo/` directory into your server's
   `game/csgo/` directory. For existing `admins.txt`, `maplist.txt`, and configuration
   files, merge only the changes you need. **Do not overwrite your administrator
   list with the empty bundled `admins.txt`.**
4. Add `3797394226` to MultiAddonManager's `mm_extra_addons` setting, preserving
   any existing addon IDs. The bundled configuration specifies only the HUD addon ID.
5. Start the server and run the following commands in the server console:

   ```text
   meta version
   meta list
   rtvhud_status
   ```

   Confirm that RTV HUD reports version `0.9.0`, `ready=1`, and `browser_assets=1`.
   With the bundled map list, it should also report `maps=364`.
6. Verify the map browser and voting in-game.

Version 0.9.0 uses the same Workshop assets as 0.8.0. If they are already installed,
you do not need to republish the assets.

## Configuration

The bundled configuration uses these defaults:

| Setting | Default |
| --- | --- |
| Required agreement (`rtvhud_required_percent`) | 100% |
| Initial wait | 30 seconds |
| Cooldown | 30 seconds |
| Voting duration | 30 seconds |
| Result display duration | 5 seconds |
| Map browser timeout | 120 seconds |
| Change to the winning map | Enabled |
| End-of-map voting | Enabled |

See [`game/csgo/cfg/rtv_hud.cfg`](game/csgo/cfg/rtv_hud.cfg) for configuration details.
After making changes, run the appropriate command in the server console:

| Change | Command |
| --- | --- |
| Map list | `rtvhud_reload` |
| Administrator list | `rtvhud_reload_admins` |
| Configuration | `exec rtv_hud.cfg` |

## Building from source

You need Linux, a C++17 compiler, CMake, Make, Python 3, and Git.
From `source/rtv-hud/`, run `bash bootstrap.sh` to fetch the pinned SDK and
Metamod dependencies.

**If you build on the same host as the CS2 server, stop CS2 before building.**
For setups using `cs2server.service`, `bash build-server.sh` stops the service,
verifies that it has stopped, builds with a single job, and runs the tests.
It requires permission to manage the systemd service and leaves the server stopped
after completion.

For other setups, stop the server using the appropriate method for your environment,
confirm that it has stopped, and run these commands from `source/rtv-hud/`:

```sh
cmake -S . -B build-khook -DCMAKE_BUILD_TYPE=Release \
  -DSDK="$PWD/deps/reference/hl2sdk-cs2" \
  -DMMS="$PWD/deps/metamod-source"
cmake --build build-khook -j1
ctest --test-dir build-khook --output-on-failure
```

The output is `build-khook/rtv_hud.so`. All eight existing tests passed during
release verification.

The Windows instructions under the Workshop directory are for compiling HUD
assets. This package does not include a Windows server plugin binary.
References to version 0.8.0 in the source tree document the history of the map
browser assets; use this README for the current installation requirements.

## License

Distributed under AGPL-3.0-or-later. See [LICENSE](LICENSE),
[NOTICE.md](NOTICE.md), and [THIRD_PARTY](THIRD_PARTY) for license and attribution details.
