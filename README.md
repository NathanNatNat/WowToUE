# WowToUE

An Unreal Engine 5.7 editor plugin that imports World of Warcraft game assets directly into UE. Browse and preview M2 models, creatures, textures, and more from a local WoW installation — no manual export step required.

## What It Does

WowToUE reads WoW's CASC archive system directly and converts assets into UE-native formats in real time:

- **CASC Browser** — Browse all 1.8M+ files in a local WoW installation across 15 categorised tabs (Models, Textures, Characters, Creatures, Items, Decor, Audio, Maps, Zones, and more)
- **M2 Model Preview** — Full 3D preview with skeletal mesh, GPU skinning, and animation playback
- **Animation System** — Play, pause, step, and scrub through all 400+ animations per model
- **Creature Display** — Resolve creature textures and geoset overrides from DB2 tables
- **Geoset Control** — Toggle individual mesh sections (armor pieces, hair, facial features)
- **BLP Textures** — Decode and preview WoW's BLP texture format
- **Skeleton Visualisation** — UE-style wireframe bone rendering with full bone hierarchy list
- **Direct Import** — Import assets as native UE skeletal meshes, textures, and materials

## Architecture

The plugin has three modules:

| Module | Purpose |
|--------|---------|
| **WowLib** | C++ port of wow.export's core: CASC access, M2/SKEL/M3/WMO/ADT loaders, DB2 parsing, BLP decoding |
| **WowToUERuntime** | UE bridge layer: coordinate conversion, model data structures |
| **WowToUEEditor** | Editor UI: CASC browser, 3D preview viewport, asset factories |

### Coordinate Conversion

WoW (right-handed Z-up) to UE (left-handed Z-up) is a direct single-step conversion with no intermediate coordinate system:

```
Position:  (X, -Y, Z) * 100
Rotation:  (-qX, qY, -qZ, qW)
Scale:     (sX, sY, sZ)
```

See [Docs/CoordinateConversion.md](Docs/CoordinateConversion.md) for full derivations and sample code.

## Inspired By

This project is built on the foundation of [wow.export](https://github.com/Kruithne/wow.export), an Electron-based WoW asset export tool. A significant portion of WowToUE's core code (CASC access, file format parsing, DB2 reading, BLP decoding) is a C++ port of wow.export's JavaScript codebase, adapted to compile within Unreal Engine's build system.

## Special Thanks

- **[Kruithne](https://github.com/Kruithne)** — Creator of [wow.export](https://github.com/Kruithne/wow.export), the tool that made WoW asset extraction accessible to everyone. WowToUE would not exist without this work.
- **[Marlamin](https://github.com/Marlamin)** — Major contributor to wow.export, creator of [wow.tools](https://wow.tools), and tireless reverse-engineer of WoW's file formats. Also maintains many of the community data resources this project depends on.

## Community Resources

WowToUE relies on several community-maintained resources for WoW file format knowledge and runtime data:

| Resource | Purpose |
|----------|---------|
| [wowdev.wiki](https://wowdev.wiki) | WoW file format documentation (M2, SKEL, WMO, ADT, BLP, CASC, DB2, etc.) |
| [wowdev/wow-listfile](https://github.com/wowdev/wow-listfile) | Community-maintained file ID to path mapping |
| [wowdev/TACTKeys](https://github.com/wowdev/TACTKeys) | CASC encryption keys |
| [wowdev/WoWDBDefs](https://github.com/wowdev/WoWDBDefs) | DB2 database schema definitions |

## Third-Party Libraries

| Library | License | Purpose |
|---------|---------|---------|
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | JSON parsing |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | MIT | HTTP/HTTPS client |
| [stb](https://github.com/nothings/stb) | Public Domain | Image loading, writing, resizing |
| [OpenSSL](https://www.openssl.org/) | Apache 2.0 | TLS support for HTTPS |

## License

[MIT](LICENSE)
