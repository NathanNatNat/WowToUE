#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "WowM2Loader.h"

class FWowModelPreviewClient : public FEditorViewportClient
{
public:
	FWowModelPreviewClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SEditorViewport>& InViewport);
	virtual void Tick(float DeltaSeconds) override;
};

class SWowModelPreview : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SWowModelPreview) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWowModelPreview();

	void SetM2Model(const FWowM2ModelData& ModelData);
	void ClearModel();

	void SetGeosetVisible(int32 SubMeshIndex, bool bVisible);
	void SetAllGeosetsVisible(bool bVisible);
	void ApplyCreatureDisplay(const FWowCreatureDisplay& Display);

	int32 GetNumSubMeshes() const { return SubMeshVisible.Num(); }
	uint16 GetSubMeshID(int32 Index) const;
	bool IsGeosetVisible(int32 Index) const;
	bool HasTextureUnit(int32 Index) const;
	FString GetGeosetLabel(int32 Index) const;

private:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

	void RebuildMesh(bool bFitCamera = false);
	UTexture2D* CreateTextureFromBLP(uint32 FileDataID, uint32 WrapFlags = 0);
	UMaterial* CreateUnlitMaterial(UTexture2D* Texture, uint16 BlendMode = 0, uint16 MaterialFlags = 0x05);

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FWowModelPreviewClient> ViewportClient;

	class UStaticMeshComponent* MeshComponent = nullptr;
	class UStaticMesh* PreviewMesh = nullptr;

	TArray<bool> SubMeshVisible;
	TArray<uint16> SubMeshIDs;
	TArray<FString> SubMeshLabels;
	FWowM2ModelData CurrentModelData;
};
