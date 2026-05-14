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
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
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

UMaterial* SWowModelPreview::InvisibleMaterial = nullptr;

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

UMaterial* SWowModelPreview::CreateCombinerMaterial(const TArray<UTexture2D*>& Textures, int32 CombinerID, int32 VertexShaderID, uint16 BlendMode, uint16 MaterialFlags, bool bNeedsAlphaControl)
{
	UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
	bool bTwoSided = (MaterialFlags & 0x04) != 0;
	bool bNoDepthTest = (MaterialFlags & 0x08) != 0;
	bool bNoDepthWrite = (MaterialFlags & 0x10) != 0;
	Mat->TwoSided = bTwoSided;

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

	if (Mat->BlendMode == BLEND_Masked)
		Mat->OpacityMaskClipValue = 0.502f;

	if (bNoDepthWrite && (Mat->BlendMode == BLEND_Translucent || Mat->BlendMode == BLEND_Additive))
		Mat->bDisableDepthTest = bNoDepthTest;

	Mat->bUsedWithSkeletalMesh = true;
	Mat->SetShadingModel(MSM_Unlit);

	// === Parameters ===
	auto* ColorParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
	ColorParam->ParameterName = TEXT("MeshColor");
	ColorParam->DefaultValue = FLinearColor::White;
	Mat->GetExpressionCollection().AddExpression(ColorParam);

	auto* AlphaParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
	AlphaParam->ParameterName = TEXT("MeshAlpha");
	AlphaParam->DefaultValue = 1.f;
	Mat->GetExpressionCollection().AddExpression(AlphaParam);

	auto* TSAParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
	TSAParam->ParameterName = TEXT("TexSampleAlpha");
	TSAParam->DefaultValue = FLinearColor(1, 1, 1);
	Mat->GetExpressionCollection().AddExpression(TSAParam);

	auto* TM0R0 = NewObject<UMaterialExpressionVectorParameter>(Mat);
	TM0R0->ParameterName = TEXT("TM0R0");
	TM0R0->DefaultValue = FLinearColor(1, 0, 0, 0);
	Mat->GetExpressionCollection().AddExpression(TM0R0);

	auto* TM0R1 = NewObject<UMaterialExpressionVectorParameter>(Mat);
	TM0R1->ParameterName = TEXT("TM0R1");
	TM0R1->DefaultValue = FLinearColor(0, 1, 0, 0);
	Mat->GetExpressionCollection().AddExpression(TM0R1);

	auto* TM1R0 = NewObject<UMaterialExpressionVectorParameter>(Mat);
	TM1R0->ParameterName = TEXT("TM1R0");
	TM1R0->DefaultValue = FLinearColor(1, 0, 0, 0);
	Mat->GetExpressionCollection().AddExpression(TM1R0);

	auto* TM1R1 = NewObject<UMaterialExpressionVectorParameter>(Mat);
	TM1R1->ParameterName = TEXT("TM1R1");
	TM1R1->DefaultValue = FLinearColor(0, 1, 0, 0);
	Mat->GetExpressionCollection().AddExpression(TM1R1);

	auto* WhiteConst = NewObject<UMaterialExpressionConstant>(Mat);
	WhiteConst->R = 1.f;
	Mat->GetExpressionCollection().AddExpression(WhiteConst);

	// === UV Computation Node ===
	// Implements all 19 vertex shader modes: UV channel selection, env mapping, texture transforms
	auto* UV0Node = NewObject<UMaterialExpressionTextureCoordinate>(Mat);
	UV0Node->CoordinateIndex = 0;
	Mat->GetExpressionCollection().AddExpression(UV0Node);

	auto* UV1Node = NewObject<UMaterialExpressionTextureCoordinate>(Mat);
	UV1Node->CoordinateIndex = 1;
	Mat->GetExpressionCollection().AddExpression(UV1Node);

	// View-space position for env mapping
	auto* WorldPosNode = NewObject<UMaterialExpressionWorldPosition>(Mat);
	WorldPosNode->WorldPositionShaderOffset = WPT_Default;
	Mat->GetExpressionCollection().AddExpression(WorldPosNode);

	auto* TransformPosNode = NewObject<UMaterialExpressionTransformPosition>(Mat);
	TransformPosNode->TransformSourceType = TRANSFORMPOSSOURCE_World;
	TransformPosNode->TransformType = TRANSFORMPOSSOURCE_View;
	TransformPosNode->Input.Expression = WorldPosNode;
	Mat->GetExpressionCollection().AddExpression(TransformPosNode);

	// View-space normal for env mapping
	auto* WorldNormalNode = NewObject<UMaterialExpressionVertexNormalWS>(Mat);
	Mat->GetExpressionCollection().AddExpression(WorldNormalNode);

	auto* TransformNormalNode = NewObject<UMaterialExpressionTransform>(Mat);
	TransformNormalNode->TransformSourceType = TRANSFORMSOURCE_World;
	TransformNormalNode->TransformType = TRANSFORM_View;
	TransformNormalNode->Input.Expression = WorldNormalNode;
	Mat->GetExpressionCollection().AddExpression(TransformNormalNode);

	// UV computation custom node — outputs float4(uv1.xy, uv2.xy), additional outputs for uv3 and edgeFade
	auto* UVNode = NewObject<UMaterialExpressionCustom>(Mat);
	UVNode->OutputType = CMOT_Float4;
	UVNode->Description = TEXT("M2UVCompute");

	UVNode->AdditionalOutputs.Empty();
	FCustomOutput UV3Out;
	UV3Out.OutputName = TEXT("OutUV3");
	UV3Out.OutputType = CMOT_Float2;
	UVNode->AdditionalOutputs.Add(UV3Out);

	FCustomOutput EdgeOut;
	EdgeOut.OutputName = TEXT("OutEdgeFade");
	EdgeOut.OutputType = CMOT_Float1;
	UVNode->AdditionalOutputs.Add(EdgeOut);

	UVNode->Inputs.Empty();
	UVNode->Inputs.SetNum(8);
	UVNode->Inputs[0].InputName = TEXT("UV0"); UVNode->Inputs[0].Input.Expression = UV0Node;
	UVNode->Inputs[1].InputName = TEXT("UV1"); UVNode->Inputs[1].Input.Expression = UV1Node;
	UVNode->Inputs[2].InputName = TEXT("VP");  UVNode->Inputs[2].Input.Expression = TransformPosNode;
	UVNode->Inputs[3].InputName = TEXT("VN");  UVNode->Inputs[3].Input.Expression = TransformNormalNode;
	UVNode->Inputs[4].InputName = TEXT("TM0R0"); UVNode->Inputs[4].Input.Expression = TM0R0;
	UVNode->Inputs[5].InputName = TEXT("TM0R1"); UVNode->Inputs[5].Input.Expression = TM0R1;
	UVNode->Inputs[6].InputName = TEXT("TM1R0"); UVNode->Inputs[6].Input.Expression = TM1R0;
	UVNode->Inputs[7].InputName = TEXT("TM1R1"); UVNode->Inputs[7].Input.Expression = TM1R1;

	// UV computation HLSL — matches WebWowViewerCpp commonM2Material.glsl calcM2VertexMat()
	UVNode->Code = FString::Printf(TEXT(
		"float2 uv0 = UV0; float2 uv1 = UV1;\n"
		"float3 vp = VP; float3 vn = VN;\n"
		"\n"
		"float3 viewDir = -normalize(vp);\n"
		"float3 refl = reflect(viewDir, normalize(vn));\n"
		"float3 envTmp = float3(refl.x, refl.y, refl.z + 1.0);\n"
		"float2 envUV = -(normalize(envTmp).xy * 0.5) + float2(0.5, 0.5);\n"
		"\n"
		"float dotClamped = saturate(dot(normalize(vp), vn));\n"
		"OutEdgeFade = saturate(2.7 * dotClamped * dotClamped - 0.4);\n"
		"\n"
		"float2 t0uv0 = float2(dot(TM0R0, float4(uv0, 0, 1)), dot(TM0R1, float4(uv0, 0, 1)));\n"
		"float2 t0uv1 = float2(dot(TM0R0, float4(uv1, 0, 1)), dot(TM0R1, float4(uv1, 0, 1)));\n"
		"float2 t1uv0 = float2(dot(TM1R0, float4(uv0, 0, 1)), dot(TM1R1, float4(uv0, 0, 1)));\n"
		"float2 t1uv1 = float2(dot(TM1R0, float4(uv1, 0, 1)), dot(TM1R1, float4(uv1, 0, 1)));\n"
		"\n"
		"float2 outUV1 = t0uv0; float2 outUV2 = float2(0,0); OutUV3 = float2(0,0);\n"
		"\n"
		"int vs = %d;\n"
		"if (vs == 0) { outUV1 = t0uv0; }\n"
		"else if (vs == 1) { outUV1 = envUV; }\n"
		"else if (vs == 2) { outUV1 = t0uv0; outUV2 = t1uv1; }\n"
		"else if (vs == 3) { outUV1 = t0uv0; outUV2 = envUV; }\n"
		"else if (vs == 4) { outUV1 = envUV; outUV2 = t0uv0; }\n"
		"else if (vs == 5) { outUV1 = envUV; outUV2 = envUV; }\n"
		"else if (vs == 6) { outUV1 = t0uv0; outUV2 = envUV; OutUV3 = t0uv0; }\n"
		"else if (vs == 7) { outUV1 = t0uv0; outUV2 = t0uv0; }\n"
		"else if (vs == 8) { outUV1 = t0uv0; outUV2 = t0uv0; OutUV3 = t0uv0; }\n"
		"else if (vs == 9) { outUV1 = t0uv0; }\n"
		"else if (vs == 10) { outUV1 = t1uv1; }\n"
		"else if (vs == 11) { outUV1 = t0uv0; outUV2 = envUV; OutUV3 = t1uv1; }\n"
		"else if (vs == 12) { outUV1 = t0uv0; outUV2 = t1uv1; }\n"
		"else if (vs == 13) { outUV1 = envUV; }\n"
		"else if (vs == 14) { outUV1 = t0uv0; outUV2 = t1uv1; OutUV3 = t0uv0; }\n"
		"else if (vs == 15) { outUV1 = t0uv0; outUV2 = t1uv1; }\n"
		"else if (vs == 16) { outUV1 = t1uv1; }\n"
		"else if (vs == 17) { outUV1 = t0uv0; }\n"
		"else if (vs == 18) { outUV1 = t0uv0; }\n"
		"\n"
		"if (vs != 9 && vs != 12 && vs != 13) OutEdgeFade = 1.0;\n"
		"return float4(outUV1, outUV2);\n"
	), VertexShaderID);

	Mat->GetExpressionCollection().AddExpression(UVNode);

	// === Extract computed UVs from the UV node ===
	// Main output = float4(uv1.xy, uv2.xy)
	// Additional output 0 = OutUV3 (float2)
	// Additional output 1 = OutEdgeFade (float)

	// Create component masks to extract UV1 and UV2 from the float4
	auto* UV1Mask = NewObject<UMaterialExpressionComponentMask>(Mat);
	UV1Mask->Input.Expression = UVNode;
	UV1Mask->R = true; UV1Mask->G = true; UV1Mask->B = false; UV1Mask->A = false;
	Mat->GetExpressionCollection().AddExpression(UV1Mask);

	auto* UV2Mask = NewObject<UMaterialExpressionComponentMask>(Mat);
	UV2Mask->Input.Expression = UVNode;
	UV2Mask->R = false; UV2Mask->G = false; UV2Mask->B = true; UV2Mask->A = true;
	Mat->GetExpressionCollection().AddExpression(UV2Mask);

	// === Texture samplers using computed UVs ===
	TArray<UMaterialExpressionTextureSample*> TexSamplers;
	for (int32 i = 0; i < FMath::Min(Textures.Num(), 4); ++i)
	{
		if (!Textures[i])
		{
			TexSamplers.Add(nullptr);
			continue;
		}
		auto* TexSample = NewObject<UMaterialExpressionTextureSample>(Mat);
		TexSample->Texture = Textures[i];
		TexSample->SamplerType = SAMPLERTYPE_Color;

		// Wire computed UV to each texture sampler
		if (i == 0)
			TexSample->Coordinates.Expression = UV1Mask;
		else if (i == 1)
			TexSample->Coordinates.Expression = UV2Mask;
		else if (i == 2)
		{
			TexSample->Coordinates.Expression = UVNode;
			TexSample->Coordinates.OutputIndex = 1; // OutUV3
		}
		else
		{
			// Tex4: case 28 samples from inUv2 directly (UV2)
			TexSample->Coordinates.Expression = UV2Mask;
		}

		Mat->GetExpressionCollection().AddExpression(TexSample);
		TexSamplers.Add(TexSample);
	}
	while (TexSamplers.Num() < 4)
		TexSamplers.Add(nullptr);

	// === Combiner Custom HLSL node ===
	auto* CustomNode = NewObject<UMaterialExpressionCustom>(Mat);
	CustomNode->OutputType = CMOT_Float4;
	CustomNode->Description = TEXT("M2Combiner");

	CustomNode->Inputs.Empty();
	int32 InputIdx = 0;

	auto AddTexInput = [&](const TCHAR* NameRGB, const TCHAR* NameA, int32 Slot) {
		CustomNode->Inputs.SetNum(InputIdx + 2);
		CustomNode->Inputs[InputIdx].InputName = NameRGB;
		CustomNode->Inputs[InputIdx].Input.Expression = TexSamplers[Slot] ? static_cast<UMaterialExpression*>(TexSamplers[Slot]) : WhiteConst;
		InputIdx++;

		CustomNode->Inputs[InputIdx].InputName = NameA;
		if (TexSamplers[Slot]) { CustomNode->Inputs[InputIdx].Input.Expression = TexSamplers[Slot]; CustomNode->Inputs[InputIdx].Input.OutputIndex = 4; }
		else { CustomNode->Inputs[InputIdx].Input.Expression = WhiteConst; }
		InputIdx++;
	};

	AddTexInput(TEXT("Tex1"), TEXT("Tex1A"), 0);
	AddTexInput(TEXT("Tex2"), TEXT("Tex2A"), 1);
	AddTexInput(TEXT("Tex3"), TEXT("Tex3A"), 2);
	AddTexInput(TEXT("Tex4"), TEXT("Tex4A"), 3);

	auto AddInput = [&](const TCHAR* Name, UMaterialExpression* Expr, int32 OutIdx = 0) {
		CustomNode->Inputs.SetNum(InputIdx + 1);
		CustomNode->Inputs[InputIdx].InputName = Name;
		CustomNode->Inputs[InputIdx].Input.Expression = Expr;
		CustomNode->Inputs[InputIdx].Input.OutputIndex = OutIdx;
		InputIdx++;
	};

	AddInput(TEXT("MC"), ColorParam);
	AddInput(TEXT("MA"), AlphaParam);
	AddInput(TEXT("TSA"), TSAParam);
	AddInput(TEXT("EdgeFade"), UVNode, 2); // OutEdgeFade from UV node

	// Build the HLSL code
	// All 37 combiner cases matching WebWowViewerCpp commonM2Material.glsl exactly
	FString Code = FString::Printf(TEXT(
		"float3 t1 = Tex1; float t1a = Tex1A;\n"
		"float3 t2 = Tex2; float t2a = Tex2A;\n"
		"float3 t3 = Tex3; float t3a = Tex3A;\n"
		"float t4a = Tex4A;\n"
		"float3 mc = MC * EdgeFade;\n"
		"float mcAlpha = MA * EdgeFade;\n"
		"float3 tsa = TSA;\n"
		"float3 diff = float3(0,0,0);\n"
		"float3 spec = float3(0,0,0);\n"
		"float da = 1.0;\n"
		"bool canDiscard = false;\n"
		"\n"
		"int c = %d;\n"
		"if (c == 0) { diff = mc * t1; }\n"
		"else if (c == 1) { diff = mc * t1; da = t1a; canDiscard = true; }\n"
		"else if (c == 2) { diff = mc * t1 * t2; da = t2a; canDiscard = true; }\n"
		"else if (c == 3) { diff = mc * t1 * t2 * 2.0; da = t2a * 2.0; canDiscard = true; }\n"
		"else if (c == 4) { diff = mc * t1 * t2 * 2.0; }\n"
		"else if (c == 5) { diff = mc * t1 * t2; }\n"
		"else if (c == 6) { diff = mc * t1 * t2; da = t1a * t2a; canDiscard = true; }\n"
		"else if (c == 7) { diff = mc * t1 * t2 * 2.0; da = t1a * t2a * 2.0; canDiscard = true; }\n"
		"else if (c == 8) { diff = mc * t1; spec = t2; da = t1a + t2a; canDiscard = true; }\n"
		"else if (c == 9) { diff = mc * t1 * t2 * 2.0; da = t1a; canDiscard = true; }\n"
		"else if (c == 10) { diff = mc * t1; spec = t2; da = t1a; canDiscard = true; }\n"
		"else if (c == 11) { diff = mc * t1 * t2; da = t1a; canDiscard = true; }\n"
		"else if (c == 12) { diff = mc * lerp(t1 * t2 * 2.0, t1, t1a); }\n"
		"else if (c == 13) { diff = mc * t1; spec = t2 * t2a; }\n"
		"else if (c == 14) { diff = mc * t1; spec = t2 * t2a * (1.0 - t1a); }\n"
		"else if (c == 15) { diff = mc * lerp(t1 * t2 * 2.0, t1, t1a); spec = t3 * t3a * tsa.b; }\n"
		"else if (c == 16) { diff = mc * t1; da = t1a; canDiscard = true; spec = t2 * t2a; }\n"
		"else if (c == 17) { diff = mc * t1; da = t1a + t2a * (0.3*t2.r+0.59*t2.g+0.11*t2.b); canDiscard = true; spec = t2 * t2a * (1.0-t1a); }\n"
		"else if (c == 18) { diff = mc * lerp(lerp(t1, t2, t2a), t1, t1a); }\n"
		"else if (c == 19) { diff = mc * lerp(t1 * t2 * 2.0, t3, t3a); }\n"
		"else if (c == 20) { diff = mc * t1; spec = t2 * t2a * tsa.g; }\n"
		"else if (c == 21) { diff = mc * t1; da = t1a + t2a; canDiscard = true; spec = t2 * (1.0-t1a); }\n"
		"else if (c == 22) { diff = mc * lerp(t1 * t2, t1, t1a); }\n"
		"else if (c == 23) { diff = mc * t1; da = t1a; canDiscard = true; spec = t2 * t2a * tsa.g; }\n"
		"else if (c == 24) { diff = mc * lerp(t1, t2, t2a); spec = t1 * t1a * tsa.r; }\n"
		"else if (c == 25) { float glow = saturate(t3a * tsa.b); diff = mc * lerp(t1*t2*2.0, t1, t1a) * (1.0-glow); spec = t3 * glow; }\n"
		"else if (c == 26) { diff = mc * lerp(lerp(float4(t1,t1a), float4(t2,t2a), saturate(tsa.g)), float4(t3,t3a), saturate(tsa.b)).rgb; da = lerp(lerp(float4(t1,t1a), float4(t2,t2a), saturate(tsa.g)), float4(t3,t3a), saturate(tsa.b)).a; canDiscard = true; }\n"
		"else if (c == 27) { diff = mc * lerp(lerp(t1*t2*2.0, t3, t3a), t1, t1a); }\n"
		"else if (c == 28) { diff = mc * lerp(lerp(float4(t1,t1a), float4(t2,t2a), saturate(tsa.g)), float4(t3,t3a), saturate(tsa.b)).rgb; da = lerp(lerp(float4(t1,t1a), float4(t2,t2a), saturate(tsa.g)), float4(t3,t3a), saturate(tsa.b)).a * t4a; canDiscard = true; }\n"
		"else if (c == 29) { diff = mc * lerp(t1, t2, t2a); }\n"
		"else if (c == 30) { diff = mc * lerp(t1 * lerp(float3(1,1,1), t2 * float3(1,1,1), t2a), t3 * float3(1,1,1), t3a); da = t1a; canDiscard = true; }\n"
		"else if (c == 31) { diff = mc * t1 * lerp(float3(1,1,1), t2 * float3(1,1,1), t2a); da = t1a; canDiscard = true; }\n"
		"else if (c == 32) { diff = mc * lerp(t1 * lerp(float3(1,1,1), t2 * float3(1,1,1), t2a), t3 * float3(1,1,1), t3a); }\n"
		"else if (c == 33) { diff = mc * t1; da = t1a; canDiscard = true; }\n"
		"else if (c == 34) { da = t1a; canDiscard = true; }\n"
		"else if (c == 35) { float4 r = float4(t1,t1a) * float4(t2,t2a) * float4(t3,t3a); diff = mc * r.rgb; da = r.a; canDiscard = true; }\n"
		"else if (c == 36) { diff = mc * t1 * t2; da = t1a * t2a; canDiscard = true; }\n"
		"\n"
	), CombinerID);

	// Final alpha output — matches WebWowViewerCpp per-blendMode logic
	Code += FString::Printf(TEXT(
		"float finalAlpha;\n"
		"int bm = %d;\n"
		"if (bm == 1) {\n"
		"  finalAlpha = canDiscard ? da : 1.0;\n"
		"} else if (bm == 0) {\n"
		"  finalAlpha = 1.0;\n"
		"} else {\n"
		"  finalAlpha = saturate(da * mcAlpha);\n"
		"}\n"
	), static_cast<int32>(BlendMode));

	Code += TEXT(
		"float3 litDiff = diff * 0.55;\n"
		"float3 result = litDiff + spec;\n"
		"return float4(result, finalAlpha);\n"
	);

	CustomNode->Code = Code;
	Mat->GetExpressionCollection().AddExpression(CustomNode);

	// Extract RGB for emissive, A for opacity/mask from the float4 output
	auto* ColorMask = NewObject<UMaterialExpressionComponentMask>(Mat);
	ColorMask->Input.Expression = CustomNode;
	ColorMask->R = true; ColorMask->G = true; ColorMask->B = true; ColorMask->A = false;
	Mat->GetExpressionCollection().AddExpression(ColorMask);

	auto* AlphaMask = NewObject<UMaterialExpressionComponentMask>(Mat);
	AlphaMask->Input.Expression = CustomNode;
	AlphaMask->R = false; AlphaMask->G = false; AlphaMask->B = false; AlphaMask->A = true;
	Mat->GetExpressionCollection().AddExpression(AlphaMask);

	Mat->GetEditorOnlyData()->EmissiveColor.Expression = ColorMask;

	if (Mat->BlendMode == BLEND_Masked)
		Mat->GetEditorOnlyData()->OpacityMask.Expression = AlphaMask;
	else if (Mat->BlendMode == BLEND_Translucent || Mat->BlendMode == BLEND_Additive)
		Mat->GetEditorOnlyData()->Opacity.Expression = AlphaMask;

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

		TArray<int32> ColorIndices, TexWeightIndices, TexWeightIndices1, TexWeightIndices2;
		TArray<uint8> TUFlags;
		for (const auto& Sub : ModelData.SubMeshes)
		{
			ColorIndices.Add(Sub.ColorIndex);
			TexWeightIndices.Add(Sub.TexWeightIndex);
			TexWeightIndices1.Add(Sub.TexWeightIndex1);
			TexWeightIndices2.Add(Sub.TexWeightIndex2);
			TUFlags.Add(Sub.TUFlags);
		}
		Animator->SetSubmeshInfo(ColorIndices, TexWeightIndices, TexWeightIndices1, TexWeightIndices2, TUFlags);
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

	// All checkbox-visible submeshes, sorted by priority then materialLayer
	TArray<int32> VisibleSubmeshIndices;
	BuiltSubmeshMap.Empty();
	for (int32 i = 0; i < MD.SubMeshes.Num(); ++i)
	{
		if (SubMeshVisible.IsValidIndex(i) && SubMeshVisible[i])
			VisibleSubmeshIndices.Add(i);
	}
	if (VisibleSubmeshIndices.Num() == 0)
		return;

	VisibleSubmeshIndices.Sort([&MD](int32 A, int32 B) {
		const auto& SA = MD.SubMeshes[A];
		const auto& SB = MD.SubMeshes[B];
		if (SA.Priority != SB.Priority) return SA.Priority < SB.Priority;
		if (SA.MaterialLayer != SB.MaterialLayer) return SA.MaterialLayer < SB.MaterialLayer;
		return A < B;
	});

	BuiltSubmeshMap = VisibleSubmeshIndices;

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
	SkAttrs.GetVertexInstanceUVs().SetNumChannels(MD.UV2s.Num() > 0 ? 2 : 1);

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
			if (static_cast<int32>(VI) < MD.UV2s.Num())
				InstUVs.Set(VIID, 1, MD.UV2s[VI]);
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
		const auto& Sub = MD.SubMeshes[OldIdx];

		// Resolve up to TextureCount textures via TextureCombos
		TArray<UTexture2D*> TexSlots;
		for (int32 t = 0; t < FMath::Min(static_cast<int32>(Sub.TextureCount), 4); ++t)
		{
			UTexture2D* Tex = nullptr;
			uint16 ComboIdx = Sub.TextureComboIndex + t;
			if (ComboIdx < MD.TextureCombos.Num())
			{
				uint16 TexIdx = MD.TextureCombos[ComboIdx];
				if (TexIdx < LoadedTextures.Num())
					Tex = LoadedTextures[TexIdx];
			}
			TexSlots.Add(Tex);
		}

		// Debug: log per-submesh texture info
		FString TexInfo;
		for (int32 t = 0; t < FMath::Min(static_cast<int32>(Sub.TextureCount), 4); ++t)
		{
			uint16 ComboIdx = Sub.TextureComboIndex + t;
			uint16 TexIdx = (ComboIdx < MD.TextureCombos.Num()) ? MD.TextureCombos[ComboIdx] : 0xFFFF;
			uint32 TexType = (TexIdx < static_cast<uint16>(MD.Textures.Num())) ? MD.Textures[TexIdx].Type : 0;
			uint32 FileID = (TexIdx < static_cast<uint16>(MD.Textures.Num())) ? MD.Textures[TexIdx].FileDataID : 0;
			TexInfo += FString::Printf(TEXT(" tex%d=[idx=%d type=%d fid=%d]"), t, TexIdx, TexType, FileID);
		}
		UE_LOG(LogWowPreview, Log, TEXT("SubMesh[%d] id=%d ps=%d vs=%d blend=%d flags=0x%x texCount=%d%s"),
			OldIdx, Sub.SubmeshID, Sub.PixelShaderID, Sub.VertexShaderID, Sub.BlendMode, Sub.MaterialFlags, Sub.TextureCount, *TexInfo);

		bool bNeedsAlpha = (Sub.ColorIndex >= 0 || Sub.TexWeightIndex >= 0);
		UMaterialInterface* Mat;
		if (TexSlots.Num() > 0 && TexSlots[0])
			Mat = CreateCombinerMaterial(TexSlots, Sub.PixelShaderID, Sub.VertexShaderID, Sub.BlendMode, Sub.MaterialFlags, bNeedsAlpha);
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

	// Create invisible material (once) for hiding sections without mesh rebuild
	if (!InvisibleMaterial)
	{
		InvisibleMaterial = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient | RF_MarkAsRootSet);
		InvisibleMaterial->BlendMode = BLEND_Masked;
		InvisibleMaterial->SetShadingModel(MSM_Unlit);
		InvisibleMaterial->bUsedWithSkeletalMesh = true;
		InvisibleMaterial->OpacityMaskClipValue = 0.5f;
		auto* ZeroConst = NewObject<UMaterialExpressionConstant>(InvisibleMaterial);
		ZeroConst->R = 0.f;
		InvisibleMaterial->GetExpressionCollection().AddExpression(ZeroConst);
		InvisibleMaterial->GetEditorOnlyData()->OpacityMask.Expression = ZeroConst;
		InvisibleMaterial->PostEditChange();
	}

	// Create MIDs for per-submesh color/alpha control
	SectionMIDs.Empty();
	SectionOriginalMats.Empty();
	SectionVisible.Empty();
	for (int32 i = 0; i < MeshComponent->GetNumMaterials(); ++i)
	{
		UMaterialInterface* BaseMat = MeshComponent->GetMaterial(i);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, MeshComponent);
		SectionMIDs.Add(MID);
		SectionOriginalMats.Add(MID);
		SectionVisible.Add(true);
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
	if (!Animator || !MeshComponent) return;

	const auto& AnimData = Animator->GetSubmeshAnimData();
	const auto& TexTransforms = Animator->GetTexTransforms();

	for (int32 Section = 0; Section < BuiltSubmeshMap.Num() && Section < SectionMIDs.Num(); ++Section)
	{
		int32 OrigIdx = BuiltSubmeshMap[Section];
		if (!AnimData.IsValidIndex(OrigIdx)) continue;

		const auto& Data = AnimData[OrigIdx];
		bool bShouldShow = Data.Alpha > 0.15f;
		bool bCurrentlyShown = SectionVisible.IsValidIndex(Section) && SectionVisible[Section];

		auto* MID = SectionMIDs[Section];

		MID->SetVectorParameterValue(TEXT("MeshColor"),
			FLinearColor(Data.Color.R, Data.Color.G, Data.Color.B));
		MID->SetScalarParameterValue(TEXT("MeshAlpha"), Data.Alpha);
		MID->SetVectorParameterValue(TEXT("TexSampleAlpha"),
			FLinearColor(Data.TexSampleAlpha.X, Data.TexSampleAlpha.Y, Data.TexSampleAlpha.Z));

		const auto& Sub = CurrentModelData.SubMeshes[OrigIdx];
		auto SetTM = [&](int32 TransIdx, const TCHAR* R0Name, const TCHAR* R1Name)
		{
			if (TransIdx >= 0 && TransIdx < TexTransforms.Num())
			{
				const auto& TM = TexTransforms[TransIdx];
				MID->SetVectorParameterValue(R0Name, FLinearColor(TM.Row0.X, TM.Row0.Y, TM.Row0.Z, TM.Row0.W));
				MID->SetVectorParameterValue(R1Name, FLinearColor(TM.Row1.X, TM.Row1.Y, TM.Row1.Z, TM.Row1.W));
			}
		};
		SetTM(Sub.TexTransformIndex0, TEXT("TM0R0"), TEXT("TM0R1"));
		SetTM(Sub.TexTransformIndex1, TEXT("TM1R0"), TEXT("TM1R1"));

		if (bShouldShow && !bCurrentlyShown)
		{
			MeshComponent->SetMaterial(Section, SectionOriginalMats[Section]);
			SectionVisible[Section] = true;
		}
		else if (!bShouldShow && bCurrentlyShown)
		{
			MeshComponent->SetMaterial(Section, InvisibleMaterial);
			SectionVisible[Section] = false;
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
