# RTV HUD 0.9.2

English | [日本語](README-ja.md)

RTV HUD is a map browser and map voting HUD plugin for Counter-Strike 2 servers
running Linux x86_64. Version 0.9.0 migrates five hooks from SourceHook to KHook.

Version 0.9.0 was verified on a server and in-game on September 12, 2026.
Version 0.9.2 mitigates excessive browser replication following reports of
`NETWORK_DISCONNECT_OVERFLOW` when opening the map list. Each page contains at
most 24 content rows and two navigation rows. Browser entities are private to
their owners and destroyed on close. The 0.9.1 admin restrictions are preserved.
The Linux binary has been rebuilt and all eight tests pass. The disconnect has
not been reproduced or confirmed resolved on a live server, and compatibility
with the latest CS2 build has not been verified.

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
to install version 0.9.2.

## Installation

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

   Confirm that RTV HUD reports version `0.9.2`, `ready=1`, and `browser_assets=1`.
   With the bundled map list, it should also report `maps=364`.
6. Verify the map browser and voting in-game.

## Configuration

`!map`, `!mapmenu`, `!mm` (including their `/` forms), and `rtvhud_map` require
an administrator listed in `game/csgo/addons/rtv_hud/admins.txt`. Add one SteamID64
per line, then run `rtvhud_reload_admins` in the server console. An empty list
denies everyone. This allowlist is independent of other plugins' permissions.
`!nominate` and `!nom` remain available to everyone.
Non-admins see no `CHANGE MAP` tab; their confirmation button reads `Nominate`.
Closing the browser destroys its entity, and reloading admins closes all open browsers.

To upgrade from 0.9.0 / 0.9.1, stop the server, replace
`game/csgo/addons/rtv_hud/bin/linuxsteamrt64/rtv_hud.so`, and restart.
Keep your existing administrator list, map list, and configuration files.
Use `Next page >>` / `<< Previous page` inside the list to navigate. All matches
remain accessible; the catalog is not truncated. Existing Workshop assets work
without rebuilding or republishing them.

If disconnects persist, collect the server log immediately before the disconnect
and the output of `version`, `meta version`, `meta list`, and `rtvhud_status`.
The recipient filter uses the same existing engine structure as the vote HUD;
changes to that structure in newer CS2 builds still need live verification.

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

## Using Windows

### Server installation

**Native Windows CS2 servers are not currently supported.** The bundled
`rtv_hud.so` is a Linux binary and cannot be loaded by a Windows server.
The source also uses Linux-specific libraries, engine signatures, and build
settings; building a Windows DLL requires a port, not just a different compiler.

For an x86_64 Windows PC, WSL2 provides a possible way to run the Linux server.
**RTV HUD has not been verified under WSL2; the following is a setup outline for
testing, not a verified deployment procedure.**

1. Install Ubuntu using an administrator PowerShell window:

   ```powershell
   wsl --install -d Ubuntu
   ```

   Restart as prompted, open Ubuntu, and create your Linux user account.
   Run `wsl --list --verbose` in PowerShell and confirm that Ubuntu uses version 2.
   See Microsoft's [WSL installation guide](https://learn.microsoft.com/en-us/windows/wsl/install).
2. Inside Ubuntu, install SteamCMD and the **Linux CS2 dedicated server**, following
   the [CS2 dedicated server guide](https://developer.valvesoftware.com/wiki/Counter-Strike_2/Dedicated_Servers).
   Keep the server in the Linux filesystem, for example `~/cs2-server`.
   A Windows CS2 installation cannot load this Linux plugin.
3. Install the Linux versions of Metamod and MultiAddonManager in that server,
   using the compatible versions listed above. Stop the server, then follow
   [Installation](#installation), placing the package files in the Linux server's
   `game/csgo/` directory. From Windows Explorer, Ubuntu files are accessible under
   `\\wsl.localhost\Ubuntu\home\<Linux-user>\`.
4. Start the Linux CS2 server from Ubuntu. Run `meta version`, `meta list`, and
   `rtvhud_status` in its console, then connect with your Windows CS2 client to
   verify the HUD and voting. Players do not install the server plugin locally.
5. For connections from other machines, configure WSL networking and the relevant
   Windows/Hyper-V firewall and router rules for your server ports, including UDP.
   See Microsoft's [WSL networking guide](https://learn.microsoft.com/en-us/windows/wsl/networking).
   Check connectivity separately from plugin loading.

To build the Linux plugin in Ubuntu, install `build-essential`, `cmake`, `make`,
`python3`, and `git`, then follow [Building from source](#building-from-source)
in the Ubuntu shell. The result remains a Linux `.so` file. Use `build-server.sh`
only if you have configured the `cs2server.service` service it expects.

### Building HUD assets on Windows

The scripts in `source/rtv-hud/workshop/` compile Workshop HUD assets; they do
not build or install the server plugin. Normal server installation uses addon
`3797394226` and does not require rebuilding or publishing these assets.

1. Install CS2 Workshop Tools and create or open an addon named `rtv_hud`.
2. Extract the full package and run `source/rtv-hud/workshop/Build-Live.cmd`.
3. When prompted, enter your CS2 installation folder, containing `game` and
   `content`, for example `D:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive`.
4. Check for `BUILD OK` and inspect the results under `workshop/build-output/`.
   For a local preview, use `Build-LocalPreview.cmd` and follow the bundled
   [local preview instructions (Japanese)](source/rtv-hud/workshop/LOCAL-PREVIEW.md).

The Windows asset build and preview procedures are not recorded as verified
in this package. Compilation does not automatically publish to Workshop.

## License

Distributed under AGPL-3.0-or-later. See [LICENSE](LICENSE),
[NOTICE.md](NOTICE.md), and [THIRD_PARTY](THIRD_PARTY) for license and attribution details.
