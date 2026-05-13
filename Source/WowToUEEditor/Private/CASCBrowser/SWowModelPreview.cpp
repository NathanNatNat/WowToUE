#include "CASCBrowser/SWowModelPreview.h"
#include "WowCASCInterface.h"
#include "WowM2Animator.h"
#include "3D/loaders/SKELLoader.h"
#include "AdvancedPreviewScene.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "SkinWeightsAttributesRef.h"
#include "StaticToSkeletalMeshConverter.h"
#include "BoneWeights.h"
#include "SkeletalDebugRendering.h"

FWowModelPreviewClient::FWowModelPreviewClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SEditorViewport>& InViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, InViewport)
{
	SetViewLocation(FVector(200, 200, 150));
	SetViewRotation(FRotator(-20, -135, 0));
	SetRealtime(true);
	EngineShowFlags.SetGrid(true);
}

void FWowModelPreviewClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	if (OnTick) OnTick(DeltaSeconds);
}

void FWowModelPreviewClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);
	if (OnDraw) OnDraw(PDI);
}

void SWowModelPreview::Construct(const FArguments& InArgs)
{
	FAdvancedPreviewScene::ConstructionValues CVS;
	CVS.bDefaultLighting = true;
	PreviewScene = MakeShared<FAdvancedPreviewScene>(CVS);
	PreviewScene->SetFloorVisibility(true);
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SWowModelPreview::~SWowModelPreview()
{
	ClearModel();
	Animator.Reset();
	if (ViewportClient.IsValid())
		ViewportClient->Viewport = nullptr;
}

TSharedRef<FEditorViewportClient> SWowModelPreview::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FWowModelPreviewClient(*PreviewScene, SharedThis(this)));
	ViewportClient->OnTick = [this](float DeltaSeconds) { TickAnimation(DeltaSeconds); };
	ViewportClient->OnDraw = [this](FPrimitiveDrawInterface* PDI) { DrawBones(PDI); };
	return ViewportClient.ToSharedRef();
}

void SWowModelPreview::ClearModel()
{
	if (MeshComponent)
	{
		PreviewScene->RemoveComponent(MeshComponent);
		MeshComponent = nullptr;
	}
	PreviewMesh = nullptr;
	PreviewSkeleton = nullptr;
	SubMeshVisible.Empty();
	SubMeshIDs.Empty();
	SubMeshLabels.Empty();
	CurrentModelData = FWowM2ModelData();
	TextureCache.Empty();
	Animator.Reset();
}

DEFINE_LOG_CATEGORY_STATIC(LogWowPreview, Log, All);

static FString GetGeosetGroupName(int32 Index, uint16 SubmeshID)
{
	static const TMap<int32, FString> Groups = {
		{0, TEXT("Hair")}, {1, TEXT("FacialA")}, {2, TEXT("FacialB")}, {3, TEXT("FacialC")},
		{4, TEXT("Gloves")}, {5, TEXT("Boots")}, {6, TEXT("Tail")}, {7, TEXT("Ears")},
		{8, TEXT("Wrists")}, {9, TEXT("Kneepads")}, {10, TEXT("Chest")}, {11, TEXT("Pants")},
		{12, TEXT("Tabard")}, {13, TEXT("Trousers")}, {14, TEXT("Loincloth")}, {15, TEXT("Cloak")},
		{16, TEXT("FacialJewelry")}, {17, TEXT("Eyeglow")}, {18, TEXT("Belt")}, {19, TEXT("Bone")},
		{20, TEXT("Feet")}, {21, TEXT("Skull")}, {22, TEXT("Torso")},
		{23, TEXT("HandAttach")}, {24, TEXT("HeadAttach")}, {25, TEXT("DHBlindfolds")},
		{26, TEXT("Shoulders")}, {27, TEXT("Helm")}, {28, TEXT("ArmUpper")},
		{29, TEXT("MechagnomeArms")}, {30, TEXT("MechagnomeLegs")}, {31, TEXT("MechagnomeFeet")},
		{32, TEXT("HeadSwap")}, {33, TEXT("Eyes")}, {34, TEXT("Eyebrows")},
		{35, TEXT("Piercings")}, {36, TEXT("Necklace")}, {37, TEXT("Headdress")},
		{38, TEXT("Tails")}, {39, TEXT("MiscAccessory")}, {40, TEXT("MiscFeature")},
		{41, TEXT("Noses")}, {42, TEXT("HairDecoA")}, {43, TEXT("HornDeco")},
		{44, TEXT("BodySize")}, {46, TEXT("Dracthyr")}, {51, TEXT("EyeglowB")},
	};

	if (SubmeshID == 0)
		return FString::Printf(TEXT("Geoset%d"), Index);

	int32 Group = SubmeshID / 100;
	int32 Variant = SubmeshID % 100;

	if (const FString* Name = Groups.Find(Group))
		return FString::Printf(TEXT("%s%d"), **Name, Variant);

	return FString::Printf(TEXT("Geoset%d_%d"), Index, Group);
}

static bool IsDefaultGeosetVisible(uint16 SubmeshID)
{
	if (SubmeshID == 0)
		return true;

	int32 Group = SubmeshID / 100;
	if (Group == 17 || Group == 35)
		return false;

	return true;
}

UTexture2D* SWowModelPreview::CreateTextureFromBLP(uint32 FileDataID, uint32 WrapFlags)
{
	if (FileDataID == 0) return nullptr;
	if (UTexture2D** Cached = TextureCache.Find(FileDataID)) return *Cached;

	FWowCASCInterface::FDecodedTexture Decoded;
	FString Error;
	if (!FWowCASCInterface::Get().DecodeBLP(FileDataID, Decoded, Error) || Decoded.Width == 0)
		return nullptr;

	UTexture2D* Tex = UTexture2D::CreateTransient(Decoded.Width, Decoded.Height, PF_B8G8R8A8);
	if (!Tex) return nullptr;

	Tex->AddToRoot();
	Tex->SRGB = true;
	Tex->AddressX = (WrapFlags & 0x1) ? TA_Wrap : TA_Clamp;
	Tex->AddressY = (WrapFlags & 0x2) ? TA_Wrap : TA_Clamp;

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	uint8* Data = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	const uint8* Src = Decoded.RGBA.GetData();
	int32 PixelCount = Decoded.Width * Decoded.Height;
	for (int32 i = 0; i < PixelCount; ++i)
	{
		Data[i * 4 + 0] = Src[i * 4 + 2];
		Data[i * 4 + 1] = Src[i * 4 + 1];
		Data[i * 4 + 2] = Src[i * 4 + 0];
		Data[i * 4 + 3] = Src[i * 4 + 3];
	}
	Mip.BulkData.Unlock();
	Tex->UpdateResource();

	TextureCache.Add(FileDataID, Tex);
	return Tex;
}

UMaterial* SWowModelPreview::CreateUnlitMaterial(UTexture2D* Texture, uint16 BlendMode, uint16 MaterialFlags, bool bNeedsAlphaControl)
{
	UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
	bool bUnlit = (MaterialFlags & 0x01) != 0;
	bool bTwoSided = (MaterialFlags & 0x04) != 0;
	Mat->TwoSided = bTwoSided;
	Mat->SetShadingModel(bUnlit ? MSM_Unlit : MSM_DefaultLit);

	switch (BlendMode)
	{
	case 0: Mat->BlendMode = BLEND_Opaque; break;
	case 1: Mat->BlendMode = BLEND_Masked; break;
	case 2: Mat->BlendMode = BLEND_Translucent; break;
	case 3: case 4: Mat->BlendMode = BLEND_Additive; break;
	case 5: case 6: Mat->BlendMode = BLEND_Modulate; break;
	case 7: Mat->BlendMode = BLEND_Additive; break;
	default: Mat->BlendMode = BLEND_Opaque; break;
	}

	Mat->bUsedWithSkeletalMesh = true;

	// MeshAlpha parameter — driven by MID for animated submesh visibility
	UMaterialExpressionScalarParameter* AlphaParam = nullptr;
	if (bNeedsAlphaControl)
	{
		AlphaParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
		AlphaParam->ParameterName = TEXT("MeshAlpha");
		AlphaParam->DefaultValue = 1.f;
		Mat->GetExpressionCollection().AddExpression(AlphaParam);
	}

	if (Texture)
	{
		auto* TexNode = NewObject<UMaterialExpressionTextureSample>(Mat);
		TexNode->Texture = Texture;
		TexNode->SamplerType = SAMPLERTYPE_Color;
		Mat->GetExpressionCollection().AddExpression(TexNode);

		if (AlphaParam)
		{
			auto* ColorMul = NewObject<UMaterialExpressionMultiply>(Mat);
			ColorMul->A.Expression = TexNode;
			ColorMul->B.Expression = AlphaParam;
			Mat->GetExpressionCollection().AddExpression(ColorMul);
			if (bUnlit) Mat->GetEditorOnlyData()->EmissiveColor.Expression = ColorMul;
			else Mat->GetEditorOnlyData()->BaseColor.Expression = ColorMul;
		}
		else
		{
			if (bUnlit) Mat->GetEditorOnlyData()->EmissiveColor.Expression = TexNode;
			else Mat->GetEditorOnlyData()->BaseColor.Expression = TexNode;
		}

		if (BlendMode == 1)
		{
			if (AlphaParam)
			{
				auto* Multiply = NewObject<UMaterialExpressionMultiply>(Mat);
				Multiply->A.Expression = TexNode;
				Multiply->A.OutputIndex = 4;
				Multiply->B.Expression = AlphaParam;
				Mat->GetExpressionCollection().AddExpression(Multiply);
				Mat->GetEditorOnlyData()->OpacityMask.Expression = Multiply;
			}
			else
			{
				Mat->GetEditorOnlyData()->OpacityMask.Expression = TexNode;
				Mat->GetEditorOnlyData()->OpacityMask.OutputIndex = 4;
			}
		}
		else if (BlendMode >= 2 && Mat->BlendMode != BLEND_Modulate)
		{
			if (AlphaParam)
			{
				auto* Multiply = NewObject<UMaterialExpressionMultiply>(Mat);
				Multiply->A.Expression = TexNode;
				Multiply->A.OutputIndex = 4;
				Multiply->B.Expression = AlphaParam;
				Mat->GetExpressionCollection().AddExpression(Multiply);
				Mat->GetEditorOnlyData()->Opacity.Expression = Multiply;
			}
			else
			{
				Mat->GetEditorOnlyData()->Opacity.Expression = TexNode;
				Mat->GetEditorOnlyData()->Opacity.OutputIndex = 4;
			}
		}
	}
	else if (AlphaParam)
	{
		if (Mat->BlendMode == BLEND_Masked)
			Mat->GetEditorOnlyData()->OpacityMask.Expression = AlphaParam;
		else if (Mat->BlendMode != BLEND_Opaque && Mat->BlendMode != BLEND_Modulate)
			Mat->GetEditorOnlyData()->Opacity.Expression = AlphaParam;
	}

	Mat->PostEditChange();
	return Mat;
}

void SWowModelPreview::SetM2Model(const FWowM2ModelData& ModelData, M2Loader* InLoader, SKELLoader* InSkelLoader, SKELLoader* InParentSkelLoader)
{
	ClearModel();
	CurrentModelData = ModelData;

	if (InLoader)
	{
		Animator = MakeShared<FWowM2Animator>();
		if (InParentSkelLoader)
			Animator->Initialize(InLoader, InParentSkelLoader, InSkelLoader);
		else
			Animator->Initialize(InLoader, InSkelLoader);

		// Pass UE-space pivots to animator
		TArray<FVector> Pivots;
		Pivots.SetNum(ModelData.Bones.Num());
		for (int32 i = 0; i < ModelData.Bones.Num(); ++i)
			Pivots[i] = FVector(ModelData.Bones[i].Pivot);
		Animator->SetUEPivots(Pivots);

		TArray<int32> ColorIndices, TexWeightIndices;
		for (const auto& Sub : ModelData.SubMeshes)
		{
			ColorIndices.Add(Sub.ColorIndex);
			TexWeightIndices.Add(Sub.TexWeightIndex);
		}
		Animator->SetSubmeshInfo(ColorIndices, TexWeightIndices);
		Animator->StopAnimation();
	}

	if (ModelData.Positions.Num() == 0 || ModelData.Triangles.Num() == 0)
		return;

	int32 NumSubs = ModelData.SubMeshes.Num();
	SubMeshIDs.SetNum(NumSubs);
	SubMeshVisible.SetNum(NumSubs);
	SubMeshAlphaVisible.SetNum(NumSubs);
	SubMeshLabels.SetNum(NumSubs);

	const auto& InitAlphas = Animator.IsValid() ? Animator->GetSubmeshAlphas() : TArray<float>();
	for (int32 i = 0; i < NumSubs; ++i)
	{
		const auto& Sub = ModelData.SubMeshes[i];
		SubMeshIDs[i] = Sub.SubmeshID;
		bool bHasTexUnit = (Sub.TextureComboIndex != 0xFFFF);
		SubMeshVisible[i] = bHasTexUnit && IsDefaultGeosetVisible(Sub.SubmeshID);
		SubMeshAlphaVisible[i] = !InitAlphas.IsValidIndex(i) || InitAlphas[i] > 0.f;
		SubMeshLabels[i] = GetGeosetGroupName(i, Sub.SubmeshID);
	}

	RebuildMesh(true);
}

void SWowModelPreview::RebuildMesh(bool bFitCamera)
{
	if (MeshComponent)
	{
		PreviewScene->RemoveComponent(MeshComponent);
		MeshComponent = nullptr;
	}
	PreviewMesh = nullptr;
	PreviewSkeleton = nullptr;

	const auto& MD = CurrentModelData;
	if (MD.Positions.Num() == 0 || MD.Triangles.Num() == 0)
		return;

	const int32 NumTriIndices = MD.Triangles.Num();
	const int32 TriCount = NumTriIndices / 3;
	const int32 VertCount = MD.Positions.Num();
	int32 BoneCount = MD.Bones.Num();

	// Resolve triangle indices
	TArray<uint32> Resolved;
	Resolved.SetNumUninitialized(NumTriIndices);
	for (int32 i = 0; i < NumTriIndices; ++i)
	{
		uint16 SkinIdx = MD.Triangles[i];
		uint16 VertIdx = (SkinIdx < MD.Indices.Num()) ? MD.Indices[SkinIdx] : 0;
		Resolved[i] = FMath::Clamp(static_cast<int32>(VertIdx), 0, VertCount - 1);
	}

	// Visible submeshes: checkbox-visible AND alpha-visible
	TArray<int32> VisibleSubmeshIndices;
	BuiltSubmeshMap.Empty();
	for (int32 i = 0; i < MD.SubMeshes.Num(); ++i)
	{
		bool bCheckbox = SubMeshVisible.IsValidIndex(i) && SubMeshVisible[i];
		bool bAlpha = !SubMeshAlphaVisible.IsValidIndex(i) || SubMeshAlphaVisible[i];
		if (bCheckbox && bAlpha)
		{
			BuiltSubmeshMap.Add(i);
			VisibleSubmeshIndices.Add(i);
		}
	}
	if (VisibleSubmeshIndices.Num() == 0)
		return;

	// Tri-to-group mapping
	TArray<int32> TriGroup;
	TriGroup.SetNumUninitialized(TriCount);
	for (int32 T = 0; T < TriCount; ++T) TriGroup[T] = -1;

	for (int32 NewIdx = 0; NewIdx < VisibleSubmeshIndices.Num(); ++NewIdx)
	{
		const auto& Sub = MD.SubMeshes[VisibleSubmeshIndices[NewIdx]];
		int32 TriStart = Sub.TriangleStart / 3;
		int32 TriEnd = TriStart + Sub.TriangleCount / 3;
		for (int32 T = TriStart; T < TriEnd && T < TriCount; ++T)
			TriGroup[T] = NewIdx;
	}

	// === Build Reference Skeleton ===
	// Models with 0 bones need a dummy root bone for skeletal mesh
	if (BoneCount == 0)
	{
		FWowBoneData DummyBone;
		DummyBone.BoneName = FName(TEXT("Root"));
		DummyBone.ParentIndex = -1;
		DummyBone.BoneID = -1;
		const_cast<FWowM2ModelData&>(MD).Bones.Add(DummyBone);
		BoneCount = 1;
	}

	// Ensure unique bone names and valid parent indices
	TSet<FName> UsedNames;
	TArray<FName> BoneNames;
	BoneNames.SetNum(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i)
	{
		FName BaseName = MD.Bones[i].BoneName;
		if (BaseName == NAME_None)
			BaseName = FName(*FString::Printf(TEXT("Bone_%d"), i));
		FName UniqueName = BaseName;
		int32 Suffix = 1;
		while (UsedNames.Contains(UniqueName))
			UniqueName = FName(*FString::Printf(TEXT("%s_%d"), *BaseName.ToString(), Suffix++));
		UsedNames.Add(UniqueName);
		BoneNames[i] = UniqueName;
	}

	// Build ref skeleton with ACTUAL bone positions (like FBX importer does)
	// Each bone's pose = local transform relative to parent (pivot offset)
	FReferenceSkeleton RefSkeleton;
	{
		FReferenceSkeletonModifier SkMod(RefSkeleton, nullptr);
		for (int32 i = 0; i < BoneCount; ++i)
		{
			FMeshBoneInfo Info;
			Info.Name = BoneNames[i];
			int32 ParentIdx = MD.Bones[i].ParentIndex;
			if (ParentIdx >= 0 && ParentIdx < i)
				Info.ParentIndex = ParentIdx;
			else if (i == 0)
				Info.ParentIndex = INDEX_NONE;
			else
				Info.ParentIndex = 0;

			// Bone pose = pivot position relative to parent (in UE space)
			FVector MyPivot = (i < MD.Bones.Num()) ? FVector(MD.Bones[i].Pivot) : FVector::ZeroVector;
			FVector ParentPivot = (Info.ParentIndex >= 0 && Info.ParentIndex < MD.Bones.Num())
				? FVector(MD.Bones[Info.ParentIndex].Pivot) : FVector::ZeroVector;
			FTransform BonePose(FQuat::Identity, MyPivot - ParentPivot);

			SkMod.Add(Info, BonePose, true);
		}
	}

	UE_LOG(LogWowPreview, Log, TEXT("Built ref skeleton: %d bones requested, %d created"),
		BoneCount, RefSkeleton.GetRawBoneNum());

	PreviewSkeleton = NewObject<USkeleton>(GetTransientPackage(), NAME_None, RF_Transient);

	// === Build FMeshDescription with skeletal attributes ===
	FMeshDescription MeshDesc;
	FSkeletalMeshAttributes SkAttrs(MeshDesc);
	SkAttrs.Register();

	TArray<FPolygonGroupID> PolyGroups;
	for (int32 i = 0; i < VisibleSubmeshIndices.Num(); ++i)
		PolyGroups.Add(MeshDesc.CreatePolygonGroup());

	auto PolyGroupNames = SkAttrs.GetPolygonGroupMaterialSlotNames();
	for (int32 i = 0; i < VisibleSubmeshIndices.Num(); ++i)
		PolyGroupNames.Set(PolyGroups[i], FName(*FString::Printf(TEXT("Material_%d"), i)));

	MeshDesc.ReserveNewVertices(VertCount);

	auto Positions = SkAttrs.GetVertexPositions();
	auto SkinWeights = SkAttrs.GetVertexSkinWeights();

	for (int32 i = 0; i < VertCount; ++i)
	{
		FVertexID VID = MeshDesc.CreateVertex();
		Positions.Set(VID, MD.Positions[i]);

		// Skin weights — clamp to actual skeleton bone count
		int32 ActualBoneCount = RefSkeleton.GetRawBoneNum();
		TArray<UE::AnimationCore::FBoneWeight> BW;
		if (MD.BoneWeights.Num() >= (i + 1) * 4 && MD.BoneIndices.Num() >= (i + 1) * 4)
		{
			for (int32 j = 0; j < 4; ++j)
			{
				uint8 BIdx = MD.BoneIndices[i * 4 + j];
				uint8 BWt = MD.BoneWeights[i * 4 + j];
				if (BWt > 0 && BIdx < ActualBoneCount)
					BW.Add(UE::AnimationCore::FBoneWeight(BIdx, BWt / 255.f));
			}
		}
		if (BW.Num() == 0)
			BW.Add(UE::AnimationCore::FBoneWeight(0, 1.0f));

		SkinWeights.Set(VID, UE::AnimationCore::FBoneWeights::Create(BW));
	}

	auto InstNormals = SkAttrs.GetVertexInstanceNormals();
	auto InstUVs = SkAttrs.GetVertexInstanceUVs();

	for (int32 Tri = 0; Tri < TriCount; ++Tri)
	{
		if (TriGroup[Tri] < 0) continue;

		const int32 Winding[3] = { 0, 1, 2 };
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(3);

		for (int32 C = 0; C < 3; ++C)
		{
			uint32 VI = Resolved[Tri * 3 + Winding[C]];
			FVertexInstanceID VIID = MeshDesc.CreateVertexInstance(FVertexID(VI));
			Corners[C] = VIID;

			if (static_cast<int32>(VI) < MD.Normals.Num())
				InstNormals[VIID] = MD.Normals[VI];
			if (static_cast<int32>(VI) < MD.UVs.Num())
				InstUVs.Set(VIID, 0, MD.UVs[VI]);
		}

		MeshDesc.CreateTriangle(PolyGroups[TriGroup[Tri]], Corners);
	}

	// === Build USkeletalMesh ===
	PreviewMesh = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewMesh->SetRefSkeleton(RefSkeleton);

	// Materials
	TArray<FSkeletalMaterial> SkMaterials;
	TArray<UTexture2D*> LoadedTextures;
	for (int32 i = 0; i < MD.Textures.Num(); ++i)
	{
		const auto& TexRef = MD.Textures[i];
		UTexture2D* Tex = (TexRef.FileDataID > 0) ? CreateTextureFromBLP(TexRef.FileDataID, TexRef.Flags) : nullptr;
		LoadedTextures.Add(Tex);
	}

	for (int32 NewIdx = 0; NewIdx < VisibleSubmeshIndices.Num(); ++NewIdx)
	{
		int32 OldIdx = VisibleSubmeshIndices[NewIdx];
		UTexture2D* TexForSlot = nullptr;
		if (OldIdx < MD.SubMeshes.Num())
		{
			uint16 ComboIdx = MD.SubMeshes[OldIdx].TextureComboIndex;
			if (ComboIdx < MD.TextureCombos.Num())
			{
				uint16 TexIdx = MD.TextureCombos[ComboIdx];
				if (TexIdx < LoadedTextures.Num())
					TexForSlot = LoadedTextures[TexIdx];
			}
		}

		const auto& Sub = MD.SubMeshes[OldIdx];
		bool bNeedsAlpha = (Sub.ColorIndex >= 0 || Sub.TexWeightIndex >= 0);
		UMaterialInterface* Mat;
		if (TexForSlot)
			Mat = CreateUnlitMaterial(TexForSlot, Sub.BlendMode, Sub.MaterialFlags, bNeedsAlpha);
		else
			Mat = UMaterial::GetDefaultMaterial(MD_Surface);

		FSkeletalMaterial SkMat;
		SkMat.MaterialInterface = Mat;
		SkMat.MaterialSlotName = FName(*FString::Printf(TEXT("Material_%d"), NewIdx));
		SkMat.ImportedMaterialSlotName = SkMat.MaterialSlotName;
		SkMaterials.Add(SkMat);
	}

	TArray<const FMeshDescription*> Descs = { &MeshDesc };
	bool bBuilt = FStaticToSkeletalMeshConverter::InitializeSkeletalMeshFromMeshDescriptions(
		PreviewMesh, Descs, SkMaterials, RefSkeleton, false, true);

	if (!bBuilt)
	{
		UE_LOG(LogWowPreview, Error, TEXT("Failed to build skeletal mesh"));
		PreviewMesh = nullptr;
		PreviewSkeleton = nullptr;
		return;
	}

	PreviewMesh->SetSkeleton(PreviewSkeleton);
	PreviewSkeleton->MergeAllBonesToBoneTree(PreviewMesh);

	// === Create UPoseableMeshComponent ===
	FRotator WowOrient(0.f, 0.f, 0.f);
	MeshComponent = NewObject<UPoseableMeshComponent>(PreviewScene->GetWorld());
	MeshComponent->SetSkinnedAssetAndUpdate(PreviewMesh);
	PreviewScene->AddComponent(MeshComponent, FTransform(WowOrient));

	// Create MIDs for per-submesh alpha control
	SectionMIDs.Empty();
	for (int32 i = 0; i < MeshComponent->GetNumMaterials(); ++i)
	{
		UMaterialInterface* BaseMat = MeshComponent->GetMaterial(i);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, MeshComponent);
		SectionMIDs.Add(MID);
		MeshComponent->SetMaterial(i, MID);
	}

	UpdateBoneTransforms();
	UpdateSubmeshAlphaVisibility();

	if (bFitCamera && ViewportClient.IsValid())
	{
		FBoxSphereBounds Bounds = PreviewMesh->GetBounds();
		float Radius = FMath::Max(Bounds.SphereRadius, 10.0f);
		ViewportClient->SetViewLocation(Bounds.Origin + FVector(Radius * 2.0f, Radius * 2.0f, Radius));
		ViewportClient->SetViewRotation(FRotator(-20, -135, 0));
		ViewportClient->SetLookAtLocation(Bounds.Origin);
	}
}

void SWowModelPreview::UpdateBoneTransforms()
{
	if (!MeshComponent) return;

	if (!bSkeletonEnabled || !Animator)
	{
		MeshComponent->RefreshBoneTransforms();
		return;
	}

	const auto& Transforms = Animator->GetBoneLocalTransforms();
	const int32 NumBones = FMath::Min(Transforms.Num(), MeshComponent->GetNumBones());

	for (int32 i = 0; i < NumBones; ++i)
		MeshComponent->BoneSpaceTransforms[i] = Transforms[i];

	MeshComponent->RefreshBoneTransforms();
}

void SWowModelPreview::SetSkeletonEnabled(bool bEnabled)
{
	bSkeletonEnabled = bEnabled;
	if (!MeshComponent || !PreviewMesh) return;

	if (!bEnabled)
	{
		if (Animator && Animator->IsPlaying())
			Animator->StopAnimation();

		const FReferenceSkeleton& RefSkel = PreviewMesh->GetRefSkeleton();
		for (int32 i = 0; i < MeshComponent->GetNumBones(); ++i)
			MeshComponent->BoneSpaceTransforms[i] = RefSkel.GetRefBonePose()[i];
		MeshComponent->RefreshBoneTransforms();
	}
	else
	{
		UpdateBoneTransforms();
	}
}

void SWowModelPreview::DrawBones(FPrimitiveDrawInterface* PDI)
{
	if (!bShowBones || !MeshComponent || !PDI) return;

	const int32 NumBones = MeshComponent->GetNumBones();

	TArray<TArray<int32>> Children;
	Children.SetNum(NumBones);
	for (int32 i = 0; i < NumBones; ++i)
	{
		int32 ParentIdx = (i < CurrentModelData.Bones.Num()) ? CurrentModelData.Bones[i].ParentIndex : -1;
		if (ParentIdx >= 0 && ParentIdx < NumBones)
			Children[ParentIdx].Add(i);
	}

	const FLinearColor BoneColor(0.9f, 0.9f, 0.9f);
	const FLinearColor ChildColor(0.8f, 0.8f, 0.8f);

	for (int32 i = 0; i < NumBones; ++i)
	{
		FTransform BoneTransform = MeshComponent->GetBoneTransform(i);

		TArray<FVector> ChildLocations;
		TArray<FLinearColor> ChildColors;
		for (int32 ChildIdx : Children[i])
		{
			ChildLocations.Add(MeshComponent->GetBoneTransform(ChildIdx).GetLocation());
			ChildColors.Add(ChildColor);
		}

		SkeletalDebugRendering::DrawWireBoneAdvanced(
			PDI, BoneTransform, ChildLocations, ChildColors,
			BoneColor, SDPG_Foreground, 1.0f, FBoneAxisDrawConfig());
	}
}

int32 SWowModelPreview::GetBoneDepth(int32 Index) const
{
	int32 Depth = 0;
	int32 Curr = Index;
	while (Curr >= 0 && Curr < CurrentModelData.Bones.Num())
	{
		int32 Parent = CurrentModelData.Bones[Curr].ParentIndex;
		if (Parent < 0 || Parent >= Curr) break;
		Curr = Parent;
		++Depth;
	}
	return Depth;
}

uint16 SWowModelPreview::GetSubMeshID(int32 Index) const
{
	return SubMeshIDs.IsValidIndex(Index) ? SubMeshIDs[Index] : 0;
}

bool SWowModelPreview::IsGeosetVisible(int32 Index) const
{
	return SubMeshVisible.IsValidIndex(Index) ? SubMeshVisible[Index] : true;
}

bool SWowModelPreview::HasTextureUnit(int32 Index) const
{
	if (!CurrentModelData.SubMeshes.IsValidIndex(Index)) return false;
	return CurrentModelData.SubMeshes[Index].TextureComboIndex != 0xFFFF;
}

FString SWowModelPreview::GetGeosetLabel(int32 Index) const
{
	return SubMeshLabels.IsValidIndex(Index) ? SubMeshLabels[Index] : TEXT("Unknown");
}

void SWowModelPreview::SetGeosetVisible(int32 SubMeshIndex, bool bVisible)
{
	if (!SubMeshVisible.IsValidIndex(SubMeshIndex)) return;
	SubMeshVisible[SubMeshIndex] = bVisible;
	RebuildMesh();
}

void SWowModelPreview::SetAllGeosetsVisible(bool bVisible)
{
	for (int32 i = 0; i < SubMeshVisible.Num(); ++i) SubMeshVisible[i] = bVisible;
	RebuildMesh();
}

void SWowModelPreview::ApplyCreatureDisplay(const FWowCreatureDisplay& Display)
{
	if (CurrentModelData.FileDataID == 0) return;

	for (int32 i = 0; i < CurrentModelData.Textures.Num(); ++i)
	{
		uint32 TextureType = CurrentModelData.Textures[i].Type;
		uint32 Slot = 0;
		bool bIsReplaceable = false;
		if (TextureType >= 11 && TextureType < 14) { Slot = TextureType - 11; bIsReplaceable = true; }
		else if (TextureType > 1 && TextureType < 5) { Slot = TextureType - 2; bIsReplaceable = true; }

		if (bIsReplaceable && static_cast<int32>(Slot) < Display.TextureFileDataIDs.Num())
		{
			uint32 ResolvedID = Display.TextureFileDataIDs[Slot];
			if (ResolvedID > 0) CurrentModelData.Textures[i].FileDataID = ResolvedID;
		}
	}

	if (Display.ExtraGeosets.Num() > 0)
	{
		TSet<uint32> Enabled(Display.ExtraGeosets);
		for (int32 i = 0; i < SubMeshIDs.Num(); ++i)
		{
			if (SubMeshIDs[i] == 0 || SubMeshIDs[i] >= 900) continue;
			SubMeshVisible[i] = Enabled.Contains(static_cast<uint32>(SubMeshIDs[i]));
		}
	}
	else
	{
		for (int32 i = 0; i < SubMeshIDs.Num(); ++i)
		{
			uint16 ID = SubMeshIDs[i];
			SubMeshVisible[i] = (ID == 0) || (ID % 10 == 0) || (ID % 100 == 1);
		}
	}

	TextureCache.Empty();
	RebuildMesh();
}

void SWowModelPreview::UpdateSubmeshAlphaVisibility()
{
	if (!Animator) return;

	const auto& Alphas = Animator->GetSubmeshAlphas();
	bool bNeedRebuild = false;

	for (int32 i = 0; i < SubMeshVisible.Num() && i < Alphas.Num(); ++i)
	{
		bool bWasAlphaVisible = !SubMeshAlphaVisible.IsValidIndex(i) || SubMeshAlphaVisible[i];
		bool bNowAlphaVisible = Alphas[i] > 0.f;
		if (bWasAlphaVisible != bNowAlphaVisible)
		{
			if (SubMeshAlphaVisible.IsValidIndex(i))
				SubMeshAlphaVisible[i] = bNowAlphaVisible;
			bNeedRebuild = true;
		}
	}

	if (bNeedRebuild)
		RebuildMesh();

	// Drive color multiplier on visible sections
	if (MeshComponent)
	{
		for (int32 Section = 0; Section < BuiltSubmeshMap.Num() && Section < SectionMIDs.Num(); ++Section)
		{
			int32 OrigIdx = BuiltSubmeshMap[Section];
			float Alpha = Alphas.IsValidIndex(OrigIdx) ? Alphas[OrigIdx] : 1.f;
			SectionMIDs[Section]->SetScalarParameterValue(TEXT("MeshAlpha"), Alpha);
		}
	}
}

void SWowModelPreview::TickAnimation(float DeltaSeconds)
{
	if (!bSkeletonEnabled || !Animator || !Animator->IsPlaying() || Animator->bPaused) return;
	Animator->Update(DeltaSeconds);
	UpdateBoneTransforms();
	UpdateSubmeshAlphaVisibility();
}

void SWowModelPreview::PlayAnimation(int32 AnimIndex)
{
	if (!Animator) return;
	Animator->PlayAnimation(AnimIndex);
	UpdateBoneTransforms();
	UpdateSubmeshAlphaVisibility();
}

void SWowModelPreview::StopAnimation()
{
	if (!Animator) return;
	Animator->StopAnimation();
	UpdateBoneTransforms();
	UpdateSubmeshAlphaVisibility();
}

void SWowModelPreview::SetAnimationPaused(bool bPaused)
{
	if (Animator) Animator->bPaused = bPaused;
}

void SWowModelPreview::SetAnimationFrame(int32 Frame)
{
	if (!Animator) return;
	Animator->SetFrame(Frame);
	UpdateBoneTransforms();
	UpdateSubmeshAlphaVisibility();
}

void SWowModelPreview::StepAnimationFrame(int32 Delta)
{
	if (!Animator) return;
	Animator->StepFrame(Delta);
	UpdateBoneTransforms();
	UpdateSubmeshAlphaVisibility();
}

int32 SWowModelPreview::GetAnimationFrame() const
{
	return Animator ? Animator->GetCurrentFrame() : 0;
}

int32 SWowModelPreview::GetAnimationFrameCount() const
{
	return Animator ? Animator->GetFrameCount() : 0;
}

bool SWowModelPreview::IsAnimationPaused() const
{
	return Animator ? Animator->bPaused : false;
}
