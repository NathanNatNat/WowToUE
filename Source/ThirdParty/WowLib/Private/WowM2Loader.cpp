#include "WowM2Loader.h"
#include "WowCASCInterface.h"
#include "Logging/LogMacros.h"

#include "3D/loaders/M2Loader.h"
#include "3D/loaders/M2Generics.h"
#include "3D/loaders/SKELLoader.h"
#include "3D/loaders/M3Loader.h"
#include "3D/Skin.h"
#include "3D/Texture.h"
#include "3D/BoneMapper.h"
#include "3D/AnimMapper.h"
#include "db/caches/DBCreatures.h"
#include "casc/listfile.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowM2, Log, All);

static void ApplyRestPoseSkinning(M2Loader& Loader)
{
	const auto& Bones = Loader.bones;
	if (Bones.empty() || Loader.boneWeights.empty() || Loader.boneIndices.empty())
		return;

	const int32 BoneCount = static_cast<int32>(Bones.size());
	const uint32 VertCount = static_cast<uint32>(Loader.vertices.size() / 3);

	// Calculate rest-pose bone matrices (animation 0, time 0)
	TArray<FMatrix> BoneMatrices;
	BoneMatrices.SetNum(BoneCount);
	TArray<bool> Calculated;
	Calculated.Init(false, BoneCount);

	// Recursive bone calculation matching wow.export's calc_bone
	TFunction<void(int32)> CalcBone = [&](int32 Idx)
	{
		if (Idx < 0 || Idx >= BoneCount || Calculated[Idx])
			return;

		const auto& Bone = Bones[Idx];

		if (Bone.parentBone >= 0 && Bone.parentBone < BoneCount)
			CalcBone(Bone.parentBone);

		float Px = Bone.pivot[0], Py = Bone.pivot[1], Pz = Bone.pivot[2];

		// Sample first keyframe of animation 0 (rest pose)
		auto SampleVec3 = [](const M2Track& Track, float DefX, float DefY, float DefZ, float* Out)
		{
			if (!Track.values.empty() && !Track.values[0].empty())
			{
				const auto* Vec = std::get_if<std::vector<float>>(&Track.values[0][0]);
				if (Vec && Vec->size() >= 3) { Out[0] = (*Vec)[0]; Out[1] = (*Vec)[1]; Out[2] = (*Vec)[2]; return; }
			}
			Out[0] = DefX; Out[1] = DefY; Out[2] = DefZ;
		};

		auto SampleQuat = [](const M2Track& Track, float* Out)
		{
			if (!Track.values.empty() && !Track.values[0].empty())
			{
				const auto* Vec = std::get_if<std::vector<float>>(&Track.values[0][0]);
				if (Vec && Vec->size() >= 4) { Out[0] = (*Vec)[0]; Out[1] = (*Vec)[1]; Out[2] = (*Vec)[2]; Out[3] = (*Vec)[3]; return; }
			}
			Out[0] = 0.f; Out[1] = 0.f; Out[2] = 0.f; Out[3] = 1.f;
		};

		bool bHasTrans = !Bone.translation.values.empty() && !Bone.translation.values[0].empty();
		bool bHasRot = !Bone.rotation.values.empty() && !Bone.rotation.values[0].empty();
		bool bHasScale = !Bone.scale.values.empty() && !Bone.scale.values[0].empty();

		FMatrix LocalMat = FMatrix::Identity;

		if (bHasTrans || bHasRot || bHasScale)
		{
			// UE row-vector convention: v * T(-pivot) * S * R * T(anim) * T(pivot)
			FMatrix Result = FTranslationMatrix(FVector(-Px, -Py, -Pz));

			if (bHasScale)
			{
				float S[3];
				SampleVec3(Bone.scale, 1, 1, 1, S);
				Result = Result * FScaleMatrix(FVector(S[0], S[1], S[2]));
			}

			if (bHasRot)
			{
				float Q[4];
				SampleQuat(Bone.rotation, Q);
				FQuat Quat(Q[0], Q[1], Q[2], Q[3]);
				Result = Result * FQuatRotationMatrix(Quat);
			}

			if (bHasTrans)
			{
				float T[3];
				SampleVec3(Bone.translation, 0, 0, 0, T);
				Result = Result * FTranslationMatrix(FVector(T[0], T[1], T[2]));
			}

			Result = Result * FTranslationMatrix(FVector(Px, Py, Pz));
			LocalMat = Result;
		}

		if (Bone.parentBone >= 0 && Bone.parentBone < BoneCount)
			BoneMatrices[Idx] = LocalMat * BoneMatrices[Bone.parentBone];
		else
			BoneMatrices[Idx] = LocalMat;

		Calculated[Idx] = true;
	};

	for (int32 i = 0; i < BoneCount; ++i)
		CalcBone(i);

	// Apply bone transforms to vertices
	for (uint32 v = 0; v < VertCount; ++v)
	{
		float Px = Loader.vertices[v * 3 + 0];
		float Py = Loader.vertices[v * 3 + 1];
		float Pz = Loader.vertices[v * 3 + 2];
		float Nx = Loader.normals[v * 3 + 0];
		float Ny = Loader.normals[v * 3 + 1];
		float Nz = Loader.normals[v * 3 + 2];

		FVector4 SrcPos(Px, Py, Pz, 1.f);
		FVector4 SrcNrm(Nx, Ny, Nz, 0.f);
		FVector4 OutPos(0, 0, 0, 0);
		FVector4 OutNrm(0, 0, 0, 0);

		for (int32 j = 0; j < 4; ++j)
		{
			uint8 BIdx = Loader.boneIndices[v * 4 + j];
			uint8 BWeight = Loader.boneWeights[v * 4 + j];
			if (BWeight == 0 || BIdx >= BoneCount)
				continue;

			float W = BWeight / 255.f;
			OutPos += BoneMatrices[BIdx].TransformFVector4(SrcPos) * W;
			OutNrm += BoneMatrices[BIdx].TransformFVector4(SrcNrm) * W;
		}

		Loader.vertices[v * 3 + 0] = OutPos.X;
		Loader.vertices[v * 3 + 1] = OutPos.Y;
		Loader.vertices[v * 3 + 2] = OutPos.Z;
		Loader.normals[v * 3 + 0] = OutNrm.X;
		Loader.normals[v * 3 + 1] = OutNrm.Y;
		Loader.normals[v * 3 + 2] = OutNrm.Z;
	}
}

bool FWowM2Loader::LoadM2(uint32 FileDataID, FWowM2ModelData& OutModel, FM2LoadResult& OutResult, FString& OutError)
{
	if (!FWowCASCInterface::Get().IsLoaded())
	{
		OutError = TEXT("CASC not loaded");
		return false;
	}

	try
	{
		auto LoaderBuf = MakeShared<BufferWrapper>(FWowCASCInterface::Get().GetFileData(FileDataID));
		auto LoaderPtr = MakeShared<M2Loader>(*LoaderBuf);
		LoaderPtr->load().get();
		auto& Loader = *LoaderPtr;

		OutModel.FileDataID = FileDataID;
		OutModel.Name = UTF8_TO_TCHAR(Loader.name.c_str());
		OutModel.BoneCount = static_cast<uint32>(Loader.bones.size());
		OutModel.AnimationCount = static_cast<uint32>(Loader.animations.size());
		OutModel.SkinCount = Loader.viewCount;

		// WoW -> UE coordinate transform (verified working for M2 models):
		// UE.X(forward) = -WoW.Y, UE.Y(right) = WoW.X, UE.Z(up) = WoW.Z
		// Scale: WoW meters -> UE centimeters (*100)
		uint32 VertCount = static_cast<uint32>(Loader.vertices.size() / 3);
		OutModel.VertexCount = VertCount;
		OutModel.Positions.SetNumUninitialized(VertCount);
		OutModel.Normals.SetNumUninitialized(VertCount);

		for (uint32 i = 0; i < VertCount; ++i)
		{
			float wx = Loader.vertices[i * 3 + 0];
			float wy = Loader.vertices[i * 3 + 1];
			float wz = Loader.vertices[i * 3 + 2];
			OutModel.Positions[i] = FVector3f(-wy * 100.f, wx * 100.f, wz * 100.f);

			float nx = Loader.normals[i * 3 + 0];
			float ny = Loader.normals[i * 3 + 1];
			float nz = Loader.normals[i * 3 + 2];
			OutModel.Normals[i] = FVector3f(-ny, nx, nz);
		}

		uint32 UVCount = static_cast<uint32>(Loader.uv.size() / 2);
		OutModel.UVs.SetNumUninitialized(UVCount);
		for (uint32 i = 0; i < UVCount; ++i)
		{
			OutModel.UVs[i] = FVector2f(Loader.uv[i * 2], Loader.uv[i * 2 + 1]);
		}

		if (!Loader.uv2.empty())
		{
			uint32 UV2Count = static_cast<uint32>(Loader.uv2.size() / 2);
			OutModel.UV2s.SetNumUninitialized(UV2Count);
			for (uint32 i = 0; i < UV2Count; ++i)
			{
				OutModel.UV2s[i] = FVector2f(Loader.uv2[i * 2], Loader.uv2[i * 2 + 1]);
			}
		}

		// Load first skin
		Skin* SkinData = Loader.getSkin(0).get();
		if (SkinData)
		{
			OutModel.Indices.Append(SkinData->indices.data(), SkinData->indices.size());
			OutModel.Triangles.Append(SkinData->triangles.data(), SkinData->triangles.size());
			OutModel.TriangleCount = static_cast<uint32>(SkinData->triangles.size() / 3);

			for (const auto& SM : SkinData->subMeshes)
			{
				FWowSubMeshData Sub;
				Sub.SubmeshID = SM.submeshID;
				Sub.TriangleStart = SM.triangleStart;
				Sub.TriangleCount = SM.triangleCount;
				Sub.TextureComboIndex = 0xFFFF;
				OutModel.SubMeshes.Add(Sub);
			}

			// Match first textureUnit per submesh (same as wow.export.cpp line 675-679)
			for (const auto& TU : SkinData->textureUnits)
			{
				if (TU.skinSectionIndex < OutModel.SubMeshes.Num())
				{
					auto& Sub = OutModel.SubMeshes[TU.skinSectionIndex];
					if (Sub.TextureComboIndex != 0xFFFF)
						continue;

					Sub.TextureComboIndex = TU.textureComboIndex;

					if (TU.materialIndex < Loader.materials.size())
					{
						const auto& Mat = Loader.materials[TU.materialIndex];
						Sub.BlendMode = Mat.blendingMode;
						Sub.MaterialFlags = Mat.flags;
					}
				}
			}
		}

		OutModel.TextureCombos.Append(Loader.textureCombos.data(), Loader.textureCombos.size());

		for (size_t i = 0; i < Loader.textures.size(); ++i)
		{
			FWowTextureRef Ref;
			Ref.FileDataID = Loader.textures[i].fileDataID;
			Ref.Type = (i < Loader.textureTypes.size()) ? Loader.textureTypes[i] : 0;
			Ref.Flags = Loader.textures[i].flags;
			Ref.FileName = UTF8_TO_TCHAR(Loader.textures[i].fileName.c_str());
			OutModel.Textures.Add(Ref);
		}

		// Load skeleton — external SKEL file if present, matching wow.export _create_skeleton()
		TSharedPtr<BufferWrapper> SkelBuf, ParentSkelBuf;
		TSharedPtr<SKELLoader> SkelLoader, ParentSkelLoader;

		if (Loader.skeletonFileID > 0)
		{
			try
			{
				SkelBuf = MakeShared<BufferWrapper>(FWowCASCInterface::Get().GetFileData(Loader.skeletonFileID));
				SkelLoader = MakeShared<SKELLoader>(*SkelBuf);
				SkelLoader->load();

				if (SkelLoader->parent_skel_file_id > 0)
				{
					ParentSkelBuf = MakeShared<BufferWrapper>(FWowCASCInterface::Get().GetFileData(SkelLoader->parent_skel_file_id));
					ParentSkelLoader = MakeShared<SKELLoader>(*ParentSkelBuf);
					ParentSkelLoader->load();
				}
			}
			catch (const std::exception& e)
			{
				UE_LOG(LogWowM2, Warning, TEXT("Failed to load skeleton: %s"), UTF8_TO_TCHAR(e.what()));
			}
		}

		// Determine bone source: parent skel > skel > m2 (matching wow.export)
		auto PopulateBones = [&](const auto& BoneSource)
		{
			for (size_t i = 0; i < BoneSource.size(); ++i)
			{
				const auto& B = BoneSource[i];
				FWowBoneData Bone;
				std::string BName = get_bone_name(B.boneID, static_cast<int>(i), B.boneNameCRC);
				Bone.BoneName = FName(UTF8_TO_TCHAR(BName.c_str()));
				Bone.ParentIndex = B.parentBone;
				Bone.BoneID = B.boneID;
				if (B.pivot.size() >= 3)
					Bone.Pivot = FVector3f(-B.pivot[1] * 100.f, B.pivot[0] * 100.f, B.pivot[2] * 100.f);
				OutModel.Bones.Add(Bone);
			}
		};

		if (ParentSkelLoader && !ParentSkelLoader->bones.empty())
			PopulateBones(ParentSkelLoader->bones);
		else if (SkelLoader && !SkelLoader->bones.empty())
			PopulateBones(SkelLoader->bones);
		else
			PopulateBones(Loader.bones);

		OutModel.BoneCount = static_cast<uint32>(OutModel.Bones.Num());

		// Determine animation source: skel > m2 (matching wow.export)
		auto PopulateAnims = [&](const auto& AnimSource)
		{
			for (size_t i = 0; i < AnimSource.size(); ++i)
			{
				const auto& A = AnimSource[i];
				FWowAnimationInfo Anim;
				Anim.AnimIndex = static_cast<int32>(i);
				Anim.AnimID = A.id;
				Anim.VariationIndex = A.variationIndex;
				Anim.DurationMs = A.duration;
				std::string AName = get_anim_name(A.id);
				Anim.Label = FString::Printf(TEXT("%s (%u.%u)"), UTF8_TO_TCHAR(AName.c_str()), A.id, A.variationIndex);
				OutModel.Animations.Add(Anim);

				if (A.id == 15 && OutModel.HandsClosedAnimIndex < 0)
					OutModel.HandsClosedAnimIndex = static_cast<int32>(i);
			}
		};

		if (SkelLoader && !SkelLoader->animations.empty())
		{
			PopulateAnims(SkelLoader->animations);
			// If parent skel exists, it's the main anim source; child overrides specific anims
			if (ParentSkelLoader && !ParentSkelLoader->animations.empty())
			{
				OutModel.Animations.Empty();
				OutModel.HandsClosedAnimIndex = -1;
				PopulateAnims(ParentSkelLoader->animations);
			}
		}
		else
			PopulateAnims(Loader.animations);

		OutModel.AnimationCount = static_cast<uint32>(OutModel.Animations.Num());

		// Bone weights and indices (4 per vertex)
		if (!Loader.boneWeights.empty())
			OutModel.BoneWeights.Append(Loader.boneWeights.data(), Loader.boneWeights.size());
		if (!Loader.boneIndices.empty())
			OutModel.BoneIndices.Append(Loader.boneIndices.data(), Loader.boneIndices.size());

		UE_LOG(LogWowM2, Log, TEXT("LoadM2(%u): %s — %d verts, %d tris, %d bones, %d anims, %d textures"),
			FileDataID, *OutModel.Name, OutModel.VertexCount, OutModel.TriangleCount,
			OutModel.BoneCount, OutModel.AnimationCount, OutModel.Textures.Num());

		OutResult.M2Buffer = LoaderBuf;
		OutResult.Loader = LoaderPtr;
		OutResult.SkelBuffer = SkelBuf;
		OutResult.SkelLoader = SkelLoader;
		OutResult.ParentSkelBuffer = ParentSkelBuf;
		OutResult.ParentSkelLoader = ParentSkelLoader;
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogWowM2, Error, TEXT("LoadM2(%u) failed: %s"), FileDataID, UTF8_TO_TCHAR(e.what()));
		OutError = UTF8_TO_TCHAR(e.what());
		return false;
	}
}

bool FWowM2Loader::LoadM3(uint32 FileDataID, FWowM2ModelData& OutModel, FString& OutError)
{
	if (!FWowCASCInterface::Get().IsLoaded())
	{
		OutError = TEXT("CASC not loaded");
		return false;
	}

	try
	{
		auto Buf = MakeShared<BufferWrapper>(FWowCASCInterface::Get().GetFileData(FileDataID));
		M3Loader Loader(*Buf);
		Loader.load();

		OutModel.FileDataID = FileDataID;

		// M3 vertices use same coordinate system as M2 raw vertices
		uint32 VertCount = static_cast<uint32>(Loader.vertices.size() / 3);
		OutModel.VertexCount = VertCount;
		OutModel.Positions.SetNumUninitialized(VertCount);
		OutModel.Normals.SetNumUninitialized(VertCount);

		for (uint32 i = 0; i < VertCount; ++i)
		{
			float wx = Loader.vertices[i * 3 + 0];
			float wy = Loader.vertices[i * 3 + 1];
			float wz = Loader.vertices[i * 3 + 2];
			OutModel.Positions[i] = FVector3f(-wy * 100.f, wx * 100.f, wz * 100.f);

			if (i * 3 + 2 < Loader.normals.size())
			{
				float nx = Loader.normals[i * 3 + 0];
				float ny = Loader.normals[i * 3 + 1];
				float nz = Loader.normals[i * 3 + 2];
				OutModel.Normals[i] = FVector3f(-ny, nx, nz);
			}
		}

		uint32 UVCount = static_cast<uint32>(Loader.uv.size() / 2);
		OutModel.UVs.SetNumUninitialized(FMath::Min(UVCount, VertCount));
		for (int32 i = 0; i < OutModel.UVs.Num(); ++i)
			OutModel.UVs[i] = FVector2f(Loader.uv[i * 2], Loader.uv[i * 2 + 1]);

		// M3 indices are direct vertex indices — no skin indirection layer
		// RebuildMesh does: vertIdx = Indices[Triangles[i]]
		// So Indices = identity mapping, Triangles = actual M3 indices
		uint16 MaxIdx = 0;
		for (size_t i = 0; i < Loader.indices.size(); ++i)
			MaxIdx = FMath::Max(MaxIdx, Loader.indices[i]);

		OutModel.Indices.SetNum(MaxIdx + 1);
		for (int32 i = 0; i <= MaxIdx; ++i)
			OutModel.Indices[i] = static_cast<uint16>(i);

		OutModel.Triangles.SetNumUninitialized(Loader.indices.size());
		for (size_t i = 0; i < Loader.indices.size(); ++i)
			OutModel.Triangles[i] = Loader.indices[i];
		OutModel.TriangleCount = static_cast<uint32>(Loader.indices.size() / 3);

		// Geosets from first LOD only
		uint32 GeosetCount = Loader.geosetCountPerLOD;
		for (uint32 i = 0; i < GeosetCount && i < static_cast<uint32>(Loader.geosets.size()); ++i)
		{
			const auto& G = Loader.geosets[i];
			FWowSubMeshData Sub;
			Sub.SubmeshID = 0;
			Sub.TriangleStart = G.indexStart;
			Sub.TriangleCount = G.indexCount;
			Sub.TextureComboIndex = 0xFFFF;
			OutModel.SubMeshes.Add(Sub);
		}

		// Use render batches to assign materials
		for (const auto& RB : Loader.renderBatches)
		{
			if (RB.geosetIndex < OutModel.SubMeshes.Num())
				OutModel.SubMeshes[RB.geosetIndex].TextureComboIndex = 0; // Flag as having a texture unit
		}

		UE_LOG(LogWowM2, Log, TEXT("LoadM3(%u): %d verts, %d tris, %d geosets"),
			FileDataID, OutModel.VertexCount, OutModel.TriangleCount, OutModel.SubMeshes.Num());

		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogWowM2, Error, TEXT("LoadM3(%u) failed: %s"), FileDataID, UTF8_TO_TCHAR(e.what()));
		OutError = UTF8_TO_TCHAR(e.what());
		return false;
	}
}

bool FWowM2Loader::ResolveCreatureTextures(FWowM2ModelData& Model, FString& OutError)
{
	if (Model.FileDataID == 0 || Model.Textures.Num() == 0)
		return true;

	bool bHasReplaceableTextures = false;
	for (const auto& Tex : Model.Textures)
	{
		if ((Tex.Type >= 11 && Tex.Type < 14) || (Tex.Type > 1 && Tex.Type < 5))
		{
			bHasReplaceableTextures = true;
			break;
		}
	}

	if (!bHasReplaceableTextures)
		return true;

	try
	{
		db::caches::DBCreatures::initializeCreatureData();

		const auto* Displays = db::caches::DBCreatures::getCreatureDisplaysByFileDataID(Model.FileDataID);
		if (!Displays || Displays->empty())
		{
			UE_LOG(LogWowM2, Log, TEXT("ResolveCreatureTextures(%u): no creature displays found"), Model.FileDataID);
			return true;
		}

		const auto& Display = Displays->front().get();
		UE_LOG(LogWowM2, Log, TEXT("ResolveCreatureTextures(%u): using display %u with %d textures"),
			Model.FileDataID, Display.ID, static_cast<int>(Display.textures.size()));

		for (int32 i = 0; i < Model.Textures.Num(); ++i)
		{
			uint32 TextureType = Model.Textures[i].Type;
			uint32 Slot = 0;
			bool bIsReplaceable = false;

			if (TextureType >= 11 && TextureType < 14)
			{
				Slot = TextureType - 11;
				bIsReplaceable = true;
			}
			else if (TextureType > 1 && TextureType < 5)
			{
				Slot = TextureType - 2;
				bIsReplaceable = true;
			}

			if (bIsReplaceable && Slot < Display.textures.size())
			{
				uint32 ResolvedID = Display.textures[Slot];
				if (ResolvedID > 0)
				{
					UE_LOG(LogWowM2, Log, TEXT("  Texture[%d] type=%u slot=%u -> FileDataID %u"), i, TextureType, Slot, ResolvedID);
					Model.Textures[i].FileDataID = ResolvedID;
				}
			}
		}

		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogWowM2, Warning, TEXT("ResolveCreatureTextures(%u) failed: %s"), Model.FileDataID, UTF8_TO_TCHAR(e.what()));
		OutError = UTF8_TO_TCHAR(e.what());
		return false;
	}
}

bool FWowM2Loader::GetCreatureDisplays(uint32 M2FileDataID, TArray<FWowCreatureDisplay>& OutDisplays, FString& OutError)
{
	try
	{
		db::caches::DBCreatures::initializeCreatureData();

		const auto* Displays = db::caches::DBCreatures::getCreatureDisplaysByFileDataID(M2FileDataID);
		if (!Displays || Displays->empty())
			return true;

		TSet<FString> SeenSkinKeys;

		for (const auto& DisplayRef : *Displays)
		{
			const auto& D = DisplayRef.get();
			if (D.textures.empty())
				continue;

			// Dedup key = first texture filename + extraGeosets (matches wow.export JS)
			FString SkinKey = FString::FromInt(D.textures[0]);
			if (D.extraGeosets.has_value())
			{
				for (uint32_t G : D.extraGeosets.value())
					SkinKey += FString::Printf(TEXT(",%u"), G);
			}
			if (SeenSkinKeys.Contains(SkinKey))
				continue;
			SeenSkinKeys.Add(SkinKey);

			FWowCreatureDisplay Entry;
			Entry.DisplayID = D.ID;

			std::vector<uint32_t> TexIDs(D.textures.begin(), D.textures.end());
			auto Names = casc::listfile::formatEntries(TexIDs);
			if (!Names.empty() && !Names[0].empty())
			{
				std::string Name = Names[0];
				size_t Slash = Name.rfind('/');
				if (Slash != std::string::npos) Name = Name.substr(Slash + 1);
				size_t Dot = Name.rfind('.');
				if (Dot != std::string::npos) Name = Name.substr(0, Dot);
				Entry.Label = FString::Printf(TEXT("%s (%u)"), UTF8_TO_TCHAR(Name.c_str()), D.ID);
			}
			else
			{
				Entry.Label = FString::Printf(TEXT("Display %u"), D.ID);
			}

			for (uint32_t Tex : D.textures)
				Entry.TextureFileDataIDs.Add(Tex);

			if (D.extraGeosets.has_value())
			{
				for (uint32_t G : D.extraGeosets.value())
					Entry.ExtraGeosets.Add(G);
			}

			OutDisplays.Add(MoveTemp(Entry));
		}

		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogWowM2, Warning, TEXT("GetCreatureDisplays(%u) failed: %s"), M2FileDataID, UTF8_TO_TCHAR(e.what()));
		OutError = UTF8_TO_TCHAR(e.what());
		return false;
	}
}
