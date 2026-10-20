# vip_bf (Plugify)

[VIP] Base Functions: on spawn, a VIP on a round allowed by vip_bf.ini's round_min gets the group's health/armor/money values applied (absolute or '+N') plus helmet and (on CT) defuser. Ported from the Pisex VIP core module VIP_BaseFunctions (vip_bf). Requires the vip core.

Plugify port (fork) of **VIP_BaseFunctions ([VIP] Base Functions)** by Pisex (Metamod (C++)).
Original: https://github.com/Pisex/cs2-vip-modules
The code was rewritten for Plugify / s2sdk; behaviour follows the original.

## Requirements
- [Plugify](https://github.com/untrustedmodders/plugify) with [s2sdk](https://github.com/untrustedmodders/plugify-plugin-s2sdk)
- Plugify dependencies: s2sdk, vip, hudmenu (optional)

## Installation
Download `vip_bf-1.1.1.zip` from Releases and extract it into `game/csgo/`.

## Building
Requires Docker. `./build.sh` builds in the Steam Runtime 3 (sniper) SDK image; the package
is written to `build/package/vip_bf`.

`plugins/vip` carry only the API headers of the plugins this one uses.

## License
GPL-3.0, see LICENSE.

