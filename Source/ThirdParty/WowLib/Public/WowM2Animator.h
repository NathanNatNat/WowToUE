// WowM2Animator — Per-frame bone animation calculator for M2 models.
//
// Matches wow.export JS M2RendererGL._update_bone_matrices() exactly.
// Calculates bone transforms from animation tracks and outputs FTransform arrays
// for use with UPoseableMeshComponent.

#pragma once

#include "CoreMinimal.h"

class M2Loader;

class WOWLIB_API FWowM2Animator
{
public:
	void Initialize(M2Loader* InLoader);

	void PlayAnimation(int32 AnimIndex);
	void StopAnimation();
	void Update(float DeltaTimeSeconds);

	float GetDuration() const;
	int32 GetFrameCount() const;
	int32 GetCurrentFrame() const;
	void SetFrame(int32 Frame);
	void StepFrame(int32 Delta);

	bool bPaused = false;

	const TArray<FTransform>& GetBoneLocalTransforms() const { return BoneLocalTransforms; }

private:
	void CalcAllBones();

	FVector SampleVec3(int32 BoneIndex, int32 TrackType, int32 AnimIdx, float TimeMs, const FVector& Default);
	FQuat SampleQuat(int32 BoneIndex, int32 AnimIdx, float TimeMs);

	M2Loader* Loader = nullptr;

	int32 CurrentAnimIndex = -1;
	float AnimationTime = 0.f;
	int32 HandsClosedAnimIndex = -1;

	TArray<FTransform> BoneLocalTransforms;
	TArray<bool> BoneCalculated;
};
