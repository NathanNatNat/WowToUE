#include "CASCBrowser/SWowModelPreview.h"
#include "WowCASCInterface.h"
#include "WowM2Animator.h"
#include "AdvancedPreviewScene.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"

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
	{
		ViewportClient->Viewport = nullptr;
	}
}

TSharedRef<FEditorViewportClient> SWowModelPreview::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FWowModelPreviewClient(*PreviewScene, SharedThis(this)));
	ViewportClient->OnTick = [this](float DeltaSeconds) { TickAnimation(DeltaSeconds); };
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
	SubMeshVisible.Empty();
	SubMeshIDs.Empty();
	SubMeshLabels.Empty();
	CurrentModelData = FWowM2ModelData();
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

	FString IDStr = FString::FromInt(SubmeshID);

	// Starts with "17" (Eyeglow) or "35" (Piercings) — hidden
	if (IDStr.Len() >= 2)
	{
		FString Prefix = IDStr.Left(2);
		if (Prefix == TEXT("17") || Prefix == TEXT("35"))
			return false;
	}

	// Ends with "01" or starts with "32" — visible
	if (IDStr.Len() >= 2 && IDStr.Right(2) == TEXT("01"))
		return true;
	if (IDStr.Len() >= 2 && IDStr.Left(2) == TEXT("32"))
		return true;

	return false;
}

UTexture2D* SWowModelPreview::CreateTextureFromBLP(uint32 FileDataID, uint32 WrapFlags)
{
	if (FileDataID == 0)
		return nullptr;

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

	return Tex;
}

UMaterial* SWowModelPreview::CreateUnlitMaterial(UTexture2D* Texture, uint16 BlendMode, uint16 MaterialFlags)
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
	case 3: // NoAlphaAdd — additive without alpha
	case 4: Mat->BlendMode = BLEND_Additive; break;
	case 5: Mat->BlendMode = BLEND_Modulate; break;
	case 6: Mat->BlendMode = BLEND_Modulate; break;
	case 7: Mat->BlendMode = BLEND_Additive; break;
	default: Mat->BlendMode = BLEND_Opaque; break;
	}

	if (Texture)
	{
		auto* TexNode = NewObject<UMaterialExpressionTextureSample>(Mat);
		TexNode->Texture = Texture;
		TexNode->SamplerType = SAMPLERTYPE_Color;
		Mat->GetExpressionCollection().AddExpression(TexNode);

		if (bUnlit)
			Mat->GetEditorOnlyData()->EmissiveColor.Expression = TexNode;
		else
			Mat->GetEditorOnlyData()->BaseColor.Expression = TexNode;

		if (BlendMode == 1)
		{
			Mat->GetEditorOnlyData()->OpacityMask.Expression = TexNode;
			Mat->GetEditorOnlyData()->OpacityMask.OutputIndex = 4; // Alpha channel
		}
		else if (BlendMode >= 2)
		{
			Mat->GetEditorOnlyData()->Opacity.Expression = TexNode;
			Mat->GetEditorOnlyData()->Opacity.OutputIndex = 4; // Alpha channel
		}
	}

	Mat->PostEditChange();
	return Mat;
}

void SWowModelPreview::SetM2Model(const FWowM2ModelData& ModelData, M2Loader* InLoader)
{
	ClearModel();
	CurrentModelData = ModelData;

	if (InLoader)
	{
		Animator = MakeShared<FWowM2Animator>();
		Animator->Initialize(InLoader);
	}

	if (ModelData.Positions.Num() == 0 || ModelData.Triangles.Num() == 0)
		return;

	int32 NumSubs = ModelData.SubMeshes.Num();
	SubMeshIDs.SetNum(NumSubs);
	SubMeshVisible.SetNum(NumSubs);
	SubMeshLabels.SetNum(NumSubs);
	for (int32 i = 0; i < NumSubs; ++i)
	{
		const auto& Sub = ModelData.SubMeshes[i];
		SubMeshIDs[i] = Sub.SubmeshID;
		bool bHasTexUnit = (Sub.TextureComboIndex != 0xFFFF);
		SubMeshVisible[i] = bHasTexUnit && IsDefaultGeosetVisible(Sub.SubmeshID);
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

	const auto& MD = CurrentModelData;
	if (MD.Positions.Num() == 0 || MD.Triangles.Num() == 0)
		return;

	const int32 NumTriIndices = MD.Triangles.Num();
	const int32 TriCount = NumTriIndices / 3;
	const int32 VertCount = MD.Positions.Num();

	TArray<uint32> Resolved;
	Resolved.SetNumUninitialized(NumTriIndices);
	for (int32 i = 0; i < NumTriIndices; ++i)
	{
		uint16 SkinIdx = MD.Triangles[i];
		uint16 VertIdx = (SkinIdx < MD.Indices.Num()) ? MD.Indices[SkinIdx] : 0;
		Resolved[i] = FMath::Clamp(static_cast<int32>(VertIdx), 0, VertCount - 1);
	}

	// Collect only visible submeshes, mapping old index to new
	TArray<int32> VisibleSubmeshIndices;
	for (int32 i = 0; i < MD.SubMeshes.Num(); ++i)
	{
		if (SubMeshVisible.IsValidIndex(i) && SubMeshVisible[i])
			VisibleSubmeshIndices.Add(i);
	}

	if (VisibleSubmeshIndices.Num() == 0)
		return;

	// Build tri-to-visible-group mapping; skip tris belonging to hidden submeshes
	TArray<int32> TriGroup;
	TriGroup.SetNumUninitialized(TriCount);
	for (int32 T = 0; T < TriCount; ++T)
		TriGroup[T] = -1;

	for (int32 NewIdx = 0; NewIdx < VisibleSubmeshIndices.Num(); ++NewIdx)
	{
		const auto& Sub = MD.SubMeshes[VisibleSubmeshIndices[NewIdx]];
		int32 TriStart = Sub.TriangleStart / 3;
		int32 TriEnd = TriStart + Sub.TriangleCount / 3;
		for (int32 T = TriStart; T < TriEnd && T < TriCount; ++T)
			TriGroup[T] = NewIdx;
	}

	FMeshDescription MeshDesc;
	FStaticMeshAttributes Attributes(MeshDesc);
	Attributes.Register();

	TArray<FPolygonGroupID> PolyGroups;
	for (int32 i = 0; i < VisibleSubmeshIndices.Num(); ++i)
		PolyGroups.Add(MeshDesc.CreatePolygonGroup());

	MeshDesc.ReserveNewVertices(VertCount);
	for (int32 i = 0; i < VertCount; ++i)
	{
		FVertexID VID = MeshDesc.CreateVertex();
		Attributes.GetVertexPositions()[VID] = MD.Positions[i];
	}

	TVertexInstanceAttributesRef<FVector3f> InstNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> InstUVs = Attributes.GetVertexInstanceUVs();

	for (int32 Tri = 0; Tri < TriCount; ++Tri)
	{
		if (TriGroup[Tri] < 0)
			continue;

		const int32 Winding[3] = { 0, 2, 1 };
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

	PreviewMesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	for (int32 i = 0; i < VisibleSubmeshIndices.Num(); ++i)
		PreviewMesh->GetStaticMaterials().Add(FStaticMaterial());
	PreviewMesh->AddSourceModel();

	FMeshDescription* MeshPtr = PreviewMesh->CreateMeshDescription(0);
	*MeshPtr = MoveTemp(MeshDesc);
	PreviewMesh->CommitMeshDescription(0);
	PreviewMesh->Build();

	// Load textures and create materials for visible submeshes
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

		const auto& Sub = MD.SubMeshes[VisibleSubmeshIndices[NewIdx]];
		PreviewMesh->GetStaticMaterials()[NewIdx].MaterialInterface = CreateUnlitMaterial(TexForSlot, Sub.BlendMode, Sub.MaterialFlags);
	}

	FRotator WowOrient(-90.f, 0.f, 0.f);
	MeshComponent = NewObject<UStaticMeshComponent>(PreviewScene->GetWorld());
	MeshComponent->SetStaticMesh(PreviewMesh);
	PreviewScene->AddComponent(MeshComponent, FTransform(WowOrient));

	if (bFitCamera && ViewportClient.IsValid())
	{
		FBoxSphereBounds Bounds = PreviewMesh->GetBounds();
		float Radius = FMath::Max(Bounds.SphereRadius, 10.0f);
		ViewportClient->SetViewLocation(Bounds.Origin + FVector(Radius * 2.0f, Radius * 2.0f, Radius));
		ViewportClient->SetViewRotation(FRotator(-20, -135, 0));
		ViewportClient->SetLookAtLocation(Bounds.Origin);
	}
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
	if (!CurrentModelData.SubMeshes.IsValidIndex(Index))
		return false;
	return CurrentModelData.SubMeshes[Index].TextureComboIndex != 0xFFFF;
}

FString SWowModelPreview::GetGeosetLabel(int32 Index) const
{
	return SubMeshLabels.IsValidIndex(Index) ? SubMeshLabels[Index] : TEXT("Unknown");
}

void SWowModelPreview::SetGeosetVisible(int32 SubMeshIndex, bool bVisible)
{
	if (!SubMeshVisible.IsValidIndex(SubMeshIndex))
		return;

	SubMeshVisible[SubMeshIndex] = bVisible;
	RebuildMesh();
}

void SWowModelPreview::SetAllGeosetsVisible(bool bVisible)
{
	for (int32 i = 0; i < SubMeshVisible.Num(); ++i)
		SubMeshVisible[i] = bVisible;

	RebuildMesh();
}

void SWowModelPreview::ApplyCreatureDisplay(const FWowCreatureDisplay& Display)
{
	if (CurrentModelData.FileDataID == 0)
		return;

	// Apply replaceable textures
	for (int32 i = 0; i < CurrentModelData.Textures.Num(); ++i)
	{
		uint32 TextureType = CurrentModelData.Textures[i].Type;
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

		if (bIsReplaceable && static_cast<int32>(Slot) < Display.TextureFileDataIDs.Num())
		{
			uint32 ResolvedID = Display.TextureFileDataIDs[Slot];
			if (ResolvedID > 0)
				CurrentModelData.Textures[i].FileDataID = ResolvedID;
		}
	}

	// Apply extra geosets: hide all geosets with ID < 900, show only specified ones
	if (Display.ExtraGeosets.Num() > 0)
	{
		TSet<uint32> EnabledGeosets(Display.ExtraGeosets);

		for (int32 i = 0; i < SubMeshIDs.Num(); ++i)
		{
			uint16 ID = SubMeshIDs[i];
			if (ID == 0 || ID >= 900)
				continue;

			SubMeshVisible[i] = EnabledGeosets.Contains(static_cast<uint32>(ID));
		}
	}
	else
	{
		// No extra geosets — reset to default visibility
		for (int32 i = 0; i < SubMeshIDs.Num(); ++i)
			SubMeshVisible[i] = IsDefaultGeosetVisible(SubMeshIDs[i]);
	}

	RebuildMesh();
}

void SWowModelPreview::TickAnimation(float DeltaSeconds)
{
	if (!Animator) return;
	Animator->Update(DeltaSeconds);
}

void SWowModelPreview::PlayAnimation(int32 AnimIndex)
{
	if (Animator) Animator->PlayAnimation(AnimIndex);
}

void SWowModelPreview::StopAnimation()
{
	if (Animator) Animator->StopAnimation();
}

void SWowModelPreview::SetAnimationPaused(bool bPaused)
{
	if (Animator) Animator->bPaused = bPaused;
}

void SWowModelPreview::SetAnimationFrame(int32 Frame)
{
	if (Animator) Animator->SetFrame(Frame);
}

void SWowModelPreview::StepAnimationFrame(int32 Delta)
{
	if (Animator) Animator->StepFrame(Delta);
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
