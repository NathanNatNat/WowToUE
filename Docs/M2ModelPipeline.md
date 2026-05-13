# M2 Model Import Pipeline

## Overview

M2 is World of Warcraft's primary model format used for characters, creatures, items, and doodads. The WowImporter plugin reads M2 files from the local WoW installation via CASC, parses them using the wow.export.cpp C++ port, and renders them in an editor preview viewport using UE's skeletal mesh system.

## File Types

| File | Purpose | Loader |
|------|---------|--------|
| `.m2` | Model header: bones, animation tracks, textures, submeshes | `M2Loader` |
| `.skin` | Skin file: vertex indices, triangles, texture units | `Skin` (via M2Loader) |
| `.skel` | External skeleton: bones, pivots, animation tracks | `SKELLoader` |
| `.anim` | External animation data: keyframes for a specific animation | Loaded by M2Loader/SKELLoader |
| `.blp` | Textures (DXT-compressed) | `FWowCASCInterface::DecodeBLP` |
| `.m3` | Alternate model format (simpler, no skeleton) | `M3Loader` |

## Skeleton Hierarchy

M2 models can have up to three skeleton sources:

1. **Inline bones** — stored in the `.m2` file directly
2. **SKEL file** — external skeleton referenced by `skeletonFileID`
3. **Parent SKEL** — for models that inherit a base skeleton (e.g., character customization variants share a race skeleton)

The structural skeleton (bone hierarchy, pivots) comes from the parent SKEL if it exists, otherwise from the SKEL, otherwise from the M2 inline bones. Animation data can come from child SKEL files that override specific animations.

```
Parent SKEL (if exists)
├── Provides bone hierarchy and pivots
├── Provides base animation set
└── Child SKEL
    └── Overrides specific animations (matched by animID + variationIndex)
```

## Geometry

### Vertex Data
- Positions: `float[3]` per vertex (raw WoW coordinates)
- Normals: `float[3]` per vertex
- UVs: `float[2]` per vertex (2 UV channels)
- Bone indices: `uint8[4]` per vertex (4 bone influences)
- Bone weights: `uint8[4]` per vertex (0-255, normalized to 0-1)

### Submesh / Geoset System

M2 models use a geoset system for toggleable mesh sections (hair styles, armor pieces, facial features). Each submesh has:
- A range of triangles
- A `submeshID` that encodes the group and variant (e.g., `1701` = group 17 "Eyeglow", variant 1)
- A texture combo index linking to the material

### Triangle Indirection

M2 uses a two-level index system:
```
Triangle index → Skin index → Vertex index
```

The skin file provides an indirection layer (`skin.indices`) that maps skin-local indices to global vertex indices.

## Textures and Materials

### Texture Types
- Type 0: Hardcoded texture (fileDataID in the M2)
- Types 2-4: Character replaceable textures (body, hair, extra)
- Types 11-13: Creature replaceable textures (skin1, skin2, skin3)

Replaceable textures are resolved at runtime from creature display info (DB2 tables).

### Material Properties
- `blendMode`: 0=Opaque, 1=AlphaKey, 2=Alpha, 3-7=Additive/Mod variants
- `materialFlags`: 0x01=Unlit, 0x04=TwoSided

## Animation System

### Animation Tracks

Each bone has three animation tracks (translation, rotation, scale). Each track contains:
- `timestamps[animIndex][]` — keyframe times in milliseconds
- `values[animIndex][]` — keyframe values

Rotation values are stored as compressed quaternions (`uint16[4]` mapped to `[-1, 1]` via `(val - 32767) / 32768`).

### Interpolation
- Type 0: Step (no interpolation)
- Type 1: Linear (lerp for vectors, slerp for quaternions)

### Bone Matrix Composition

Per bone, per frame (in WoW coordinate space):
```
local = T(pivot) * T(anim_trans) * R(anim_rot) * S(anim_scale) * T(-pivot)
world = parent_world * local
```

Bones with no animation data for the current animation produce an identity local matrix.

### Global Sequences

Some bones use global sequence timers instead of animation-specific time. These are identified by the bone's `globalSequence` field and loop independently of the current animation.

### Hand Closing

Bones with IDs 8-17 (finger bones) can be overridden with animation 15 ("HandsClosed") at time 0 for grip poses.

## UE Rendering

### Skeletal Mesh

The preview uses `USkeletalMesh` + `UPoseableMeshComponent`:
- `FReferenceSkeleton` built from bone pivots (translation-only poses)
- `FMeshDescription` with skeletal attributes for geometry
- `BoneSpaceTransforms` updated per frame from the animator

### Coordinate Conversion

See [CoordinateConversion.md](CoordinateConversion.md) for the full WoW→UE transform reference.

### Preview Controls

| Control | Function |
|---------|----------|
| Show Bones | Toggle UE-style wireframe bone visualization |
| Enable Skeleton | Toggle skeletal deformation (off = raw static mesh) |
| Geoset checkboxes | Toggle individual submesh visibility |
| Skin selector | Switch between skin files |
| Animation dropdown | Select animation by name and ID |
| Play/Pause/Step/Scrub | Animation playback controls |
| Creature display | Apply creature textures and geoset overrides |
