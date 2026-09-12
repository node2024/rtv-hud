This prototype is distributed under AGPL-3.0-or-later. See LICENSE.

Native HUD schema collection handling and Linux engine signatures were adapted from
KZGlobalTeam/cs2kz-metamod v0.0.167 (AGPL-3.0):
https://github.com/KZGlobalTeam/cs2kz-metamod/tree/v0.0.167
Reference commit: bde8b06b30d459bf280cee250e9d92417f4201ce

Relevant reference files: src/sdk/entity/ccscustomhudlayout.h,
src/utils/schema.h, src/kz/hud/layout/entity.cpp, gamedata/cs2kz-core.games.txt.
The gamedata credits CS2Fixes for these engine function signatures.

RTV state machine, adapter, tests and Panorama assets were written for this prototype.
Compiled binaries also include Valve/AlliedModders SDK sources and interfaces.a;
their original licenses/notices remain in the pinned SDK checkout. No KZ plugin
runtime or CounterStrikeSharp runtime is required.

Build dependencies:
- hl2sdk-cs2: bd17582be4bc18970e4fe4c518359872fda8276d
- metamod-source: 7e24ce9 (KHook / plugin API 18)

KHook inline hook adapters: Kenzzer/KHook revision 1e200e4 (zLib license).
See THIRD_PARTY/KHook-LICENSE for the original notice.
