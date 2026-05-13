#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "WowM2Loader.h"

class FWowM2Animator;
class M2Loader;

class FWowModelPreviewClient : public FEditorViewportClient
{
public:
	FWowModelPreviewClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SEditorViewport>& InViewport);
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	TFunction<void(float)> OnTick;
	TFunction<void(FPrimitiveDrawInterface*)> OnDraw;
};

class SWowModelPreview : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SWowModelPreview) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWowModelPreview();

	void SetM2Model(const FWowM2ModelData& ModelData, M2Loader* InLoader, SKELLoader* InSkelLoader = nullptr, SKELLoader* InParentSkelLoader = nullptr);
	void ClearModel();

	void SetGeosetVisible(int32 SubMeshIndex, bool bVisible);
	void SetAllGeosetsVisible(bool bVisible);
	void ApplyCreatureDisplay(const FWowCreatureDisplay& Display);

	int32 GetNumSubMeshes() const { return SubMeshVisible.Num(); }
	uint16 GetSubMeshID(int32 Index) const;
	bool IsGeosetVisible(int32 Index) const;
	bool HasTextureUnit(int32 Index) const;
	FString GetGeosetLabel(int32 Index) const;

	void PlayAnimation(int32 AnimIndex);
	void StopAnimation();
	void SetAnimationPaused(bool bPaused);
	void SetAnimationFrame(int32 Frame);
	void StepAnimationFrame(int32 Delta);
	int32 GetAnimationFrame() const;
	int32 GetAnimationFrameCount() const;
	bool IsAnimationPaused() const;

	bool bShowBones = false;
	bool bSkeletonEnabled = true;
	void SetSkeletonEnabled(bool bEnabled);

	int32 GetNumBones() const { return CurrentModelData.Bones.Num(); }
	FName GetBoneName(int32 Index) const { return CurrentModelData.Bones.IsValidIndex(Index) ? CurrentModelData.Bones[Index].BoneName : NAME_None; }
	int32 GetBoneParent(int32 Index) const { return CurrentModelData.Bones.IsValidIndex(Index) ? CurrentModelData.Bones[Index].ParentIndex : -1; }
	int32 GetBoneDepth(int32 Index) const;

private:
	void DrawBones(FPrimitiveDrawInterface* PDI);
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

	void RebuildMesh(bool bFitCamera = false);
	void UpdateBoneTransforms();
	void TickAnimation(float DeltaSeconds);
	UTexture2D* CreateTextureFromBLP(uint32 FileDataID, uint32 WrapFlags = 0);
	UMaterial* CreateCombinerMaterial(const TArray<UTexture2D*>& Textures, int32 CombinerID, int32 VertexShaderID, uint16 BlendMode, uint16 MaterialFlags, bool bNeedsAlphaControl);

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FWowModelPreviewClient> ViewportClient;

	class UPoseableMeshComponent* MeshComponent = nullptr;
	class USkeletalMesh* PreviewMesh = nullptr;
	class USkeleton* PreviewSkeleton = nullptr;

	void UpdateSubmeshAlphaVisibility();

	TMap<uint32, UTexture2D*> TextureCache;
	TArray<bool> SubMeshVisible;
	TArray<bool> SubMeshAlphaVisible;
	TArray<uint16> SubMeshIDs;
	TArray<int32> BuiltSubmeshMap;
	TArray<UMaterialInstanceDynamic*> SectionMIDs;
	TArray<FString> SubMeshLabels;
	FWowM2ModelData CurrentModelData;

	TSharedPtr<FWowM2Animator> Animator;
};
