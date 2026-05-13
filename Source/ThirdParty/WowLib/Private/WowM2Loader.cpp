#include "WowM2Loader.h"
#include "WowCASCInterface.h"
#include "Logging/LogMacros.h"

#include "3D/loaders/M2Loader.h"
#include "3D/Skin.h"
#include "3D/Texture.h"
#include "db/caches/DBCreatures.h"
#include "casc/listfile.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowM2, Log, All);

bool FWowM2Loader::LoadM2(uint32 FileDataID, FWowM2ModelData& OutModel, FString& OutError)
{
	if (!FWowCASCInterface::Get().IsLoaded())
	{
		OutError = TEXT("CASC not loaded");
		return false;
	}

	try
	{
		BufferWrapper Buf = FWowCASCInterface::Get().GetFileData(FileDataID);
		M2Loader Loader(Buf);
		Loader.load().get();

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

		UE_LOG(LogWowM2, Log, TEXT("LoadM2(%u): %s — %d verts, %d tris, %d bones, %d anims, %d textures"),
			FileDataID, *OutModel.Name, OutModel.VertexCount, OutModel.TriangleCount,
			OutModel.BoneCount, OutModel.AnimationCount, OutModel.Textures.Num());

		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogWowM2, Error, TEXT("LoadM2(%u) failed: %s"), FileDataID, UTF8_TO_TCHAR(e.what()));
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

		TSet<uint32> SeenPrimaryTextures;

		for (const auto& DisplayRef : *Displays)
		{
			const auto& D = DisplayRef.get();
			if (D.textures.empty())
				continue;

			uint32 PrimaryTex = D.textures[0];
			if (SeenPrimaryTextures.Contains(PrimaryTex))
				continue;
			SeenPrimaryTextures.Add(PrimaryTex);

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
