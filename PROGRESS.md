# WowToUE — Current Progress

Last updated: 2026-05-13

## Completed

### CASC Browser
- Local WoW installation reading via CASC (1.88M files)
- 15 browsing tabs: Models, Textures, Characters, Creatures, Items, Item Sets, Decor, Audio, Maps, Zones, Text, Fonts, Data, Raw Files, Settings
- File search with filters (M2, WMO, M3, ID)
- Settings persistence (install path, connection state)

### M2 Model Preview
- Full 3D viewport with FAdvancedPreviewScene + SEditorViewport
- USkeletalMesh + UPoseableMeshComponent with GPU skinning
- Direct WoW-to-UE coordinate conversion (no intermediate coordinate system)
- Correct geometry with 4 bone influences per vertex
- Triangle winding handled automatically by handedness flip
- BLP texture decoding and material creation
- Material blend modes (Opaque, AlphaKey, Alpha, Additive variants)
- Two-sided and unlit material flags

### Skeleton and Animation
- External skeleton (SKEL) loading with parent/child hierarchy
- Animation playback: dropdown selector, play/pause, frame step, scrub bar
- Bone calculation matching wow.export frame-by-frame
- Direct WoW-to-UE bone transform conversion (position, rotation, scale)
- Rest pose correct (bind pose recalculated after pivots are set)
- Hand closing animation override for finger bones
- Global sequence support
- Child SKEL animation overrides (per animID + variationIndex)

### Geosets and Skins
- Submesh/geoset toggle with labelled checkboxes
- Default visibility based on geoset group rules
- Multiple skin file selection
- Enable All / Disable All controls

### Creature Display
- Creature display info resolution from DB2 tables
- Replaceable texture mapping (creature skin1/skin2/skin3)
- Extra geoset overrides per display
- Skin selector for creature variants

### M3 Models
- M3 model loading and preview (simpler format, no skeleton)

### Debug and Inspection
- UE-style wireframe bone rendering (SkeletalDebugRendering::DrawWireBoneAdvanced)
- Collapsible bone hierarchy list with depth indentation
- Enable Skeleton toggle (view raw static mesh without bone deformation)
- Show Bones toggle

### Documentation
- Coordinate conversion reference with formulas, derivations, and sample code
- M2 model pipeline documentation (format, skeleton, animation, rendering)

## In Progress

_Nothing currently in progress._

## Planned

### Asset Import
- Import M2 models as native UE assets (USkeletalMesh + USkeleton + UAnimSequence)
- Import BLP textures as UTexture2D assets
- Import materials with correct blend modes and properties
- Batch import support

### WMO (World Map Objects)
- WMO file loading and preview
- Group/portal rendering
- Doodad placement
- WMO-specific materials and lighting

### ADT (Terrain)
- ADT terrain tile loading
- Height map to UE landscape conversion
- Terrain texturing (alpha maps + texture layers)
- Water and liquid rendering

### Additional Features
- Wireframe overlay view
- UV layout viewer
- Animation export to UAnimSequence
- Equipment attachment and preview
- Character customisation preview
