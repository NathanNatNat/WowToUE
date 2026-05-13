// WowM2Loader — Parses M2 model files from CASC into UE-ready geometry data.
//
// This is the asset interpretation layer. It reads raw bytes via FWowCASCInterface::GetFileData(),
// runs them through wow.export.cpp's M2Loader, and outputs UE-friendly structs with coordinate
// transforms already applied.
//
// Pattern for future loaders: FWowWMOLoader, FWowADTLoader follow the same static-method approach.
// Each loader lives in this module as a separate file from WowCASCInterface.

#pragma once

#include "CoreMinimal.h"

class BufferWrapper;
class M2Loader;

struct WOWLIB_API FWowSubMeshData
{
	uint16 SubmeshID;
	uint32 TriangleStart;
	uint16 TriangleCount;
	uint16 TextureComboIndex;
	uint16 BlendMode = 0;    // 0=Opaque, 1=AlphaKey, 2=Alpha, 3=NoAlphaAdd, 4=Add, 5=Mod, 6=Mod2x, 7=BlendAdd
	uint16 MaterialFlags = 0; // 0x01=unlit, 0x04=no cull, 0x08=no depth test, 0x10=no depth write
};

struct WOWLIB_API FWowTextureRef
{
	uint32 FileDataID;
	uint32 Type;   // 0=normal, 2-4=char replaceable, 11-13=creature replaceable
	uint32 Flags;  // Wrap modes: 0x1=horizontal, 0x2=vertical
	FString FileName;
};

struct WOWLIB_API FWowBoneData
{
	FName BoneName;
	int32 ParentIndex = -1;
	FVector3f Pivot;
	int32 BoneID = -1;
};

struct WOWLIB_API FWowAnimationInfo
{
	int32 AnimIndex = 0;
	uint16 AnimID = 0;
	uint16 VariationIndex = 0;
	uint32 DurationMs = 0;
	FString Label;
};

struct WOWLIB_API FWowM2ModelData
{
	uint32 FileDataID = 0;
	FString Name;
	TArray<FVector3f> Positions;
	TArray<FVector3f> Normals;
	TArray<FVector2f> UVs;
	TArray<FVector2f> UV2s;
	TArray<uint16> Indices;
	TArray<uint16> Triangles;
	TArray<FWowSubMeshData> SubMeshes;
	TArray<FWowTextureRef> Textures;
	TArray<uint16> TextureCombos;
	TArray<FWowBoneData> Bones;
	TArray<FWowAnimationInfo> Animations;
	TArray<uint8> BoneWeights;
	TArray<uint8> BoneIndices;
	int32 HandsClosedAnimIndex = -1;
	uint32 VertexCount = 0;
	uint32 TriangleCount = 0;
	uint32 BoneCount = 0;
	uint32 AnimationCount = 0;
	uint32 SkinCount = 0;
};

struct WOWLIB_API FWowCreatureDisplay
{
	uint32 DisplayID;
	FString Label;
	TArray<uint32> TextureFileDataIDs;
	TArray<uint32> ExtraGeosets;
};

// Stateless M2 loader — all methods are static. Uses FWowCASCInterface for CASC file access
// and wow.export.cpp's M2Loader/DBCreatures internally.
class WOWLIB_API FWowM2Loader
{
public:
	static bool LoadM2(uint32 FileDataID, FWowM2ModelData& OutModel, TSharedPtr<BufferWrapper>& OutBuffer, TSharedPtr<M2Loader>& OutLoader, FString& OutError);
	static bool ResolveCreatureTextures(FWowM2ModelData& Model, FString& OutError);
	static bool GetCreatureDisplays(uint32 M2FileDataID, TArray<FWowCreatureDisplay>& OutDisplays, FString& OutError);
};
