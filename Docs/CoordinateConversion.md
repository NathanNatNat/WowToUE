# WoW to Unreal Engine Coordinate Conversion

## Coordinate Systems

### World of Warcraft (raw M2/SKEL data)
- **Right-handed**, Z-up
- X = right, Y = forward (into screen), Z = up
- Units: game units (roughly meters)

### Unreal Engine
- **Left-handed**, Z-up
- X = forward, Y = right, Z = up
- Units: centimeters

The two systems share the same up axis (Z) but differ in handedness and forward direction. The conversion negates Y to flip handedness and scales by 100 for unit conversion.

## Conversion Formulas

### Positions and Translations

```
UE = (WoW_X, -WoW_Y, WoW_Z) * 100
```

The Y negation flips from right-handed to left-handed. The scale converts game units to centimeters.

```cpp
// WowM2Loader.cpp — vertex positions
OutModel.Positions[i] = FVector3f(rx * 100.f, -ry * 100.f, rz * 100.f);

// WowM2Animator.cpp — animation translations
static FVector WowToUE_Translation(float x, float y, float z)
{
    return FVector(x * 100.0, -y * 100.0, z * 100.0);
}
```

### Normals

Same axis mapping as positions, no scale factor:

```
UE = (WoW_X, -WoW_Y, WoW_Z)
```

```cpp
OutModel.Normals[i] = FVector3f(nx, -ny, nz);
```

### Rotations (Quaternions)

The coordinate change matrix C has determinant -1 (reflection), so quaternion imaginary components are negated and transformed:

```
UE = (-WoW_qX, WoW_qY, -WoW_qZ, WoW_qW)
```

Derivation: for a reflection matrix C = diag(1, -1, 1), the quaternion transforms as `q_new = -C * q_imaginary`, giving `(-qX, qY, -qZ)` with W unchanged.

```cpp
static FQuat WowToUE_Rotation(float qx, float qy, float qz, float qw)
{
    return FQuat(-qx, qy, -qz, qw);
}
```

### Scale

Scale is magnitude-only per axis. Since the transformation is a simple Y negation (no axis swapping), scale components map directly:

```
UE = (WoW_sX, WoW_sY, WoW_sZ)
```

```cpp
static FVector WowToUE_Scale(float sx, float sy, float sz)
{
    return FVector(sx, sy, sz);
}
```

### Triangle Winding

The Y negation (det = -1) reverses triangle winding from CCW (WoW/OpenGL) to CW (UE/DirectX front faces). No explicit winding reversal is needed in the mesh builder.

```cpp
// SWowModelPreview.cpp — triangle indices used directly, no {0,2,1} reversal
const int32 Winding[3] = { 0, 1, 2 };
```

## Data Flow

```
Raw M2/SKEL binary
    |
    v
M2Loader / SKELLoader (wow.export.cpp)
    Reads raw WoW coordinates — no coordinate conversion applied.
    Vertices, normals, bone pivots, animation tracks stored as-is.
    |
    v
WowM2Loader.cpp (UE bridge)
    Applies WoW→UE conversion to positions, normals, pivots.
    Builds FWowM2ModelData with UE-space geometry.
    |
    v
WowM2Animator.cpp (per-frame)
    Reads raw WoW animation tracks from M2Loader/SKELLoader.
    Applies WoW→UE conversion to sampled TRS values.
    Outputs FTransform array for UPoseableMeshComponent.
    |
    v
SWowModelPreview.cpp (rendering)
    Builds USkeletalMesh from UE-space geometry.
    Copies FTransform array to BoneSpaceTransforms each frame.
```

## Bone Transform Pipeline

### Reference Skeleton (bind pose)

Each bone's ref pose is a translation-only transform: `pivot_i - parent_pivot_i` in UE space. No rotation in the ref pose.

```cpp
FTransform BonePose(FQuat::Identity, MyPivot - ParentPivot);
```

### Animation Bone Calculation

The M2 bone formula (in WoW space, matching wow.export):

```
local = T(pivot) * T(anim_trans) * R(anim_rot) * S(anim_scale) * T(-pivot)
world = parent_world * local
```

For UE's BoneSpaceTransforms (parent-relative), the math simplifies to:

```
BST[i] = FTransform(rot_UE, pivotOffset + trans_UE, scale_UE)
```

Where:
- `pivotOffset` = bone pivot minus parent pivot (in UE space)
- `trans_UE` = WoW animation translation converted to UE
- `rot_UE` = WoW animation rotation converted to UE
- `scale_UE` = WoW animation scale (unchanged)

When no animation data exists for a bone, the BST equals the ref pose:

```
BST[i] = FTransform(Identity, pivotOffset)
```

## Historical Notes

The wow.export.cpp C++ port originally applied a WebGL swizzle `(X, Z, -Y)` at load time (matching the JS wow.export which targets WebGL/glTF output). This created a two-step pipeline: WoW→WebGL→UE. The WebGL intermediate was removed in favor of direct WoW→UE conversion for simplicity and clarity.

The equivalent two-step formulas were:
- WebGL swizzle: `(X, Z, -Y)` for positions, `(qX, qZ, -qY, qW)` for rotations
- WebGL→UE: `(X, Z, Y)` for positions (UE ConvertVec3), `(-qX, -qZ, -qY, qW)` for rotations (UE ConvertQuat)
- Combined: identical to the direct `(X, -Y, Z)` / `(-qX, qY, -qZ, qW)` formulas
