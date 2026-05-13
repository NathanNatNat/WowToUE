// WowM2Animator — Per-frame bone animation calculator for M2 models.
//
// Matches wow.export JS M2RendererGL._update_bone_matrices() exactly.
// Handles external skeleton files (SKELLoader) for models that use them.
// Calculates bone transforms from animation tracks and outputs FTransform arrays
// for use with UPoseableMeshComponent.

#pragma once

#include "CoreMinimal.h"

class M2Loader;
class SKELLoader;

class WOWLIB_API FWowM2Animator
{
public:
	void Initialize(M2Loader* InLoader, SKELLoader* InSkelLoader = nullptr, SKELLoader* InChildSkelLoader = nullptr);
	void SetUEPivots(const TArray<FVector>& InPivots);

	void PlayAnimation(int32 AnimIndex);
	void StopAnimation();
	void Update(float DeltaTimeSeconds);

	float GetDuration() const;
	int32 GetFrameCount() const;
	int32 GetCurrentFrame() const;
	void SetFrame(int32 Frame);
	void StepFrame(int32 Delta);

	bool bPaused = false;
	bool bCloseRightHand = false;
	bool bCloseLeftHand = false;

	const TArray<FTransform>& GetBoneLocalTransforms() const { return BoneLocalTransforms; }
	bool IsPlaying() const { return bIsPlaying; }

private:
	void CalcAllBones();

	FVector SampleVec3(int32 BoneIndex, int32 TrackType, int32 AnimIdx, float TimeMs, const FVector& Default);
	FQuat SampleQuat(int32 BoneIndex, int32 AnimIdx, float TimeMs);

	M2Loader* Loader = nullptr;
	SKELLoader* SkelLoader = nullptr;
	SKELLoader* ChildSkelLoader = nullptr;
	SKELLoader* CurrentAnimSource = nullptr;

	int32 CurrentAnimation = -1;   // Original requested animation index
	int32 CurrentAnimIndex = -1;   // Resolved index in the animation source
	float AnimationTime = 0.f;
	int32 HandsClosedAnimIndex = -1;
	bool bIsPlaying = false;

	int32 BoneCount = 0;
	TArray<FVector> UEPivots;
	TArray<int32> BoneParentIndices;
	TArray<int32> BoneIDs;
	TArray<float> GlobalSeqTimes;
	TArray<FTransform> BoneLocalTransforms;
	TArray<bool> BoneCalculated;
};
