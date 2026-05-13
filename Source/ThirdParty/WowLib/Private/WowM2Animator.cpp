#include "WowM2Animator.h"
#include "3D/loaders/M2Loader.h"
#include "3D/loaders/M2Generics.h"
#include "3D/loaders/SKELLoader.h"

void FWowM2Animator::Initialize(M2Loader* InLoader, SKELLoader* InSkelLoader, SKELLoader* InChildSkelLoader)
{
	Loader = InLoader;
	SkelLoader = InSkelLoader;
	ChildSkelLoader = InChildSkelLoader;
	if (!Loader) return;

	if (InSkelLoader && InSkelLoader->parent_skel_file_id > 0 && InChildSkelLoader)
		BoneCount = static_cast<int32>(InSkelLoader->bones.size());
	else if (InSkelLoader && !InSkelLoader->bones.empty())
		BoneCount = static_cast<int32>(InSkelLoader->bones.size());
	else
		BoneCount = static_cast<int32>(Loader->bones.size());

	HandsClosedAnimIndex = -1;
	if (SkelLoader && !SkelLoader->animations.empty())
	{
		for (size_t i = 0; i < SkelLoader->animations.size(); ++i)
			if (SkelLoader->animations[i].id == 15) { HandsClosedAnimIndex = static_cast<int32>(i); break; }
	}
	else if (Loader)
	{
		for (size_t i = 0; i < Loader->animations.size(); ++i)
			if (Loader->animations[i].id == 15) { HandsClosedAnimIndex = static_cast<int32>(i); break; }
	}

	BoneLocalTransforms.SetNum(BoneCount);
	BoneCalculated.SetNum(BoneCount);

	// Initialize global sequence times
	const auto& GlobalLoops = (SkelLoader) ? SkelLoader->globalLoops : Loader->globalLoops;
	GlobalSeqTimes.SetNumZeroed(GlobalLoops.size());

	StopAnimation();
}

// Helpers
static int32 GetAnimCount(M2Loader* Loader, SKELLoader* SkelLoader)
{
	if (SkelLoader && !SkelLoader->animations.empty())
		return static_cast<int32>(SkelLoader->animations.size());
	return Loader ? static_cast<int32>(Loader->animations.size()) : 0;
}

void FWowM2Animator::PlayAnimation(int32 AnimIndex)
{
	if (!Loader) return;

	// Determine animation source — child SKEL overrides matching animations
	CurrentAnimSource = SkelLoader;
	int32 ResolvedIndex = AnimIndex;

	if (ChildSkelLoader && SkelLoader)
	{
		int32 AnimCount = static_cast<int32>(SkelLoader->animations.size());
		if (AnimIndex >= 0 && AnimIndex < AnimCount)
		{
			uint16_t TargetID = SkelLoader->animations[AnimIndex].id;
			uint16_t TargetVar = SkelLoader->animations[AnimIndex].variationIndex;

			for (size_t i = 0; i < ChildSkelLoader->animations.size(); ++i)
			{
				if (ChildSkelLoader->animations[i].id == TargetID &&
					ChildSkelLoader->animations[i].variationIndex == TargetVar)
				{
					CurrentAnimSource = ChildSkelLoader;
					ResolvedIndex = static_cast<int32>(i);
					break;
				}
			}
		}
	}

	if (CurrentAnimSource && ResolvedIndex >= 0)
		CurrentAnimSource->loadAnimsForIndex(ResolvedIndex);
	else if (Loader && ResolvedIndex >= 0)
		Loader->loadAnimsForIndex(ResolvedIndex).get();

	CurrentAnimation = AnimIndex;
	CurrentAnimIndex = ResolvedIndex;
	AnimationTime = 0.f;
	bIsPlaying = true;

	// Reset global sequence times for the new animation source
	const auto& GlobalLoops = CurrentAnimSource ? CurrentAnimSource->globalLoops
		: (SkelLoader ? SkelLoader->globalLoops : Loader->globalLoops);
	GlobalSeqTimes.SetNumZeroed(GlobalLoops.size());

	CalcAllBones();
}

void FWowM2Animator::StopAnimation()
{
	AnimationTime = 0.f;
	bPaused = false;
	GlobalSeqTimes.SetNumZeroed(GlobalSeqTimes.Num());

	// Render rest pose (anim 0 at time 0) then null out state
	if (BoneCount > 0)
	{
		int32 PrevAnim = CurrentAnimation;
		int32 PrevAnimIdx = CurrentAnimIndex;
		auto* PrevSource = CurrentAnimSource;

		CurrentAnimation = 0;
		CurrentAnimIndex = 0;
		CurrentAnimSource = SkelLoader;
		CalcAllBones();

		CurrentAnimation = -1;
		CurrentAnimIndex = -1;
		CurrentAnimSource = nullptr;
	}

	bIsPlaying = false;
}

void FWowM2Animator::Update(float DeltaTimeSeconds)
{
	if (!bIsPlaying || BoneCount == 0)
		return;

	if (!bPaused)
	{
		AnimationTime += DeltaTimeSeconds;

		// Advance global sequence times
		const auto& GlobalLoops = CurrentAnimSource ? CurrentAnimSource->globalLoops
			: (SkelLoader ? SkelLoader->globalLoops : Loader->globalLoops);
		for (int32 i = 0; i < GlobalSeqTimes.Num() && i < static_cast<int32>(GlobalLoops.size()); ++i)
		{
			GlobalSeqTimes[i] += DeltaTimeSeconds * 1000.f;
			if (GlobalLoops[i] > 0)
				GlobalSeqTimes[i] = FMath::Fmod(GlobalSeqTimes[i], static_cast<float>(GlobalLoops[i]));
		}
	}

	float Duration = GetDuration();
	if (Duration > 0.f)
		AnimationTime = FMath::Fmod(AnimationTime, Duration);

	CalcAllBones();
}

float FWowM2Animator::GetDuration() const
{
	if (CurrentAnimSource && CurrentAnimIndex >= 0 && CurrentAnimIndex < static_cast<int32>(CurrentAnimSource->animations.size()))
		return CurrentAnimSource->animations[CurrentAnimIndex].duration / 1000.f;
	if (SkelLoader && CurrentAnimIndex >= 0 && CurrentAnimIndex < static_cast<int32>(SkelLoader->animations.size()))
		return SkelLoader->animations[CurrentAnimIndex].duration / 1000.f;
	if (Loader && CurrentAnimIndex >= 0 && CurrentAnimIndex < static_cast<int32>(Loader->animations.size()))
		return Loader->animations[CurrentAnimIndex].duration / 1000.f;
	return 0.f;
}

int32 FWowM2Animator::GetFrameCount() const
{
	return FMath::Max(1, FMath::FloorToInt32(GetDuration() * 60.f));
}

int32 FWowM2Animator::GetCurrentFrame() const
{
	float Duration = GetDuration();
	if (Duration <= 0.f) return 0;
	return FMath::FloorToInt32((AnimationTime / Duration) * GetFrameCount());
}

void FWowM2Animator::SetFrame(int32 Frame)
{
	int32 Count = GetFrameCount();
	if (Count <= 0) return;
	AnimationTime = (static_cast<float>(Frame) / Count) * GetDuration();
	CalcAllBones();
}

void FWowM2Animator::StepFrame(int32 Delta)
{
	int32 Frame = GetCurrentFrame() + Delta;
	int32 Count = GetFrameCount();
	if (Frame < 0) Frame = Count - 1;
	else if (Frame >= Count) Frame = 0;
	SetFrame(Frame);
}

// WowLib space {x, y, z} → UE space: cyclic permutation {z*100, x*100, y*100}
static FVector WowLibToUE_Translation(float x, float y, float z)
{
	return FVector(z * 100.0, x * 100.0, y * 100.0);
}

static FQuat WowLibToUE_Rotation(float qx, float qy, float qz, float qw)
{
	return FQuat(qz, qx, qy, qw);
}

static FVector WowLibToUE_Scale(float sx, float sy, float sz)
{
	return FVector(sz, sx, sy);
}

static int32 FindKeyframe(const std::vector<M2Value>& Timestamps, float TimeMs)
{
	int32 Lo = 0, Hi = static_cast<int32>(Timestamps.size()) - 1;
	while (Lo < Hi)
	{
		int32 Mid = (Lo + Hi + 1) >> 1;
		float T = 0.f;
		if (auto* V = std::get_if<uint32_t>(&Timestamps[Mid])) T = static_cast<float>(*V);
		else if (auto* V2 = std::get_if<int16_t>(&Timestamps[Mid])) T = static_cast<float>(*V2);
		if (T <= TimeMs) Lo = Mid;
		else Hi = Mid - 1;
	}
	return Lo;
}

static float GetTimestampMs(const M2Value& V)
{
	if (auto* P = std::get_if<uint32_t>(&V)) return static_cast<float>(*P);
	if (auto* P = std::get_if<int16_t>(&V)) return static_cast<float>(*P);
	return 0.f;
}

static FVector GetVec3(const M2Value& V, const FVector& Def = FVector::ZeroVector)
{
	if (auto* P = std::get_if<std::vector<float>>(&V))
		if (P->size() >= 3) return FVector((*P)[0], (*P)[1], (*P)[2]);
	return Def;
}

static FQuat GetQuat(const M2Value& V)
{
	if (auto* P = std::get_if<std::vector<float>>(&V))
		if (P->size() >= 4) return FQuat((*P)[0], (*P)[1], (*P)[2], (*P)[3]);
	return FQuat::Identity;
}

// Get animation track from the correct source (CurrentAnimSource → SkelLoader → Loader)
static const M2Track* GetBoneTrack(M2Loader* Loader, SKELLoader* SkelLoader, SKELLoader* AnimSource, int32 BoneIndex, int32 TrackType)
{
	if (AnimSource && BoneIndex < static_cast<int32>(AnimSource->bones.size()))
	{
		if (TrackType == 0) return &AnimSource->bones[BoneIndex].translation;
		if (TrackType == 1) return &AnimSource->bones[BoneIndex].scale;
		return &AnimSource->bones[BoneIndex].rotation;
	}
	if (SkelLoader && BoneIndex < static_cast<int32>(SkelLoader->bones.size()))
	{
		if (TrackType == 0) return &SkelLoader->bones[BoneIndex].translation;
		if (TrackType == 1) return &SkelLoader->bones[BoneIndex].scale;
		return &SkelLoader->bones[BoneIndex].rotation;
	}
	if (Loader && BoneIndex < static_cast<int32>(Loader->bones.size()))
	{
		if (TrackType == 0) return &Loader->bones[BoneIndex].translation;
		if (TrackType == 1) return &Loader->bones[BoneIndex].scale;
		return &Loader->bones[BoneIndex].rotation;
	}
	return nullptr;
}

FVector FWowM2Animator::SampleVec3(int32 BoneIndex, int32 TrackType, int32 AnimIdx, float TimeMs, const FVector& Default)
{
	const M2Track* Track = GetBoneTrack(Loader, SkelLoader, CurrentAnimSource, BoneIndex, TrackType);
	if (!Track) return Default;

	if (AnimIdx < 0 || AnimIdx >= static_cast<int32>(Track->timestamps.size()))
		return Default;

	const auto& Timestamps = Track->timestamps[AnimIdx];
	const auto& Values = Track->values[AnimIdx];
	if (Timestamps.empty() || Values.empty())
		return Default;

	if (Timestamps.size() == 1 || TimeMs <= GetTimestampMs(Timestamps[0]))
		return GetVec3(Values[0], Default);

	if (TimeMs >= GetTimestampMs(Timestamps.back()))
		return GetVec3(Values.back(), Default);

	int32 Frame = FindKeyframe(Timestamps, TimeMs);
	float T0 = GetTimestampMs(Timestamps[Frame]);
	float T1 = GetTimestampMs(Timestamps[Frame + 1]);
	float Dt = T1 - T0;
	float Alpha = Dt > 0.f ? FMath::Min((TimeMs - T0) / Dt, 1.f) : 0.f;

	FVector V0 = GetVec3(Values[Frame], Default);
	FVector V1 = GetVec3(Values[Frame + 1], Default);
	return FMath::Lerp(V0, V1, Alpha);
}

FQuat FWowM2Animator::SampleQuat(int32 BoneIndex, int32 AnimIdx, float TimeMs)
{
	const M2Track* TrackPtr = GetBoneTrack(Loader, SkelLoader, CurrentAnimSource, BoneIndex, 2);
	if (!TrackPtr) return FQuat::Identity;

	const auto& Track = *TrackPtr;

	if (AnimIdx < 0 || AnimIdx >= static_cast<int32>(Track.timestamps.size()))
		return FQuat::Identity;

	const auto& Timestamps = Track.timestamps[AnimIdx];
	const auto& Values = Track.values[AnimIdx];
	if (Timestamps.empty() || Values.empty())
		return FQuat::Identity;

	if (Timestamps.size() == 1 || TimeMs <= GetTimestampMs(Timestamps[0]))
		return GetQuat(Values[0]);

	if (TimeMs >= GetTimestampMs(Timestamps.back()))
		return GetQuat(Values.back());

	int32 Frame = FindKeyframe(Timestamps, TimeMs);
	float T0 = GetTimestampMs(Timestamps[Frame]);
	float T1 = GetTimestampMs(Timestamps[Frame + 1]);
	float Dt = T1 - T0;
	float Alpha = Dt > 0.f ? FMath::Min((TimeMs - T0) / Dt, 1.f) : 0.f;

	FQuat Q0 = GetQuat(Values[Frame]);
	FQuat Q1 = GetQuat(Values[Frame + 1]);
	return FQuat::Slerp(Q0, Q1, Alpha);
}

void FWowM2Animator::CalcAllBones()
{
	if (BoneCount == 0) return;

	const float TimeMs = AnimationTime * 1000.f;
	const int32 AnimIdx = CurrentAnimIndex;

	BoneLocalTransforms.SetNum(BoneCount);
	BoneCalculated.SetNum(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i) BoneCalculated[i] = false;

	TArray<FMatrix> ComponentMatrices;
	ComponentMatrices.SetNum(BoneCount);

	// Hand grip: only apply if the flag is set (matching wow.export close_r/close_l)
	const bool bCloseR = bCloseRightHand && HandsClosedAnimIndex >= 0;
	const bool bCloseL = bCloseLeftHand && HandsClosedAnimIndex >= 0;

	TFunction<void(int32)> CalcBone = [&](int32 Idx)
	{
		if (Idx < 0 || Idx >= BoneCount || BoneCalculated[Idx])
			return;

		// Structural bone data (pivot, parent, boneID)
		float Bpx = 0, Bpy = 0, Bpz = 0;
		int32 ParentIdx = -1;
		int32 BoneID = -1;

		if (SkelLoader && Idx < static_cast<int32>(SkelLoader->bones.size()))
		{
			const auto& B = SkelLoader->bones[Idx];
			ParentIdx = B.parentBone;
			BoneID = B.boneID;
			if (B.pivot.size() >= 3) { Bpx = B.pivot[0]; Bpy = B.pivot[1]; Bpz = B.pivot[2]; }
		}
		else if (Loader && Idx < static_cast<int32>(Loader->bones.size()))
		{
			const auto& B = Loader->bones[Idx];
			ParentIdx = B.parentBone;
			BoneID = B.boneID;
			if (B.pivot.size() >= 3) { Bpx = B.pivot[0]; Bpy = B.pivot[1]; Bpz = B.pivot[2]; }
		}

		if (ParentIdx >= 0 && ParentIdx < BoneCount)
			CalcBone(ParentIdx);

		float Px = Bpx, Py = Bpy, Pz = Bpz;

		// Determine effective animation — finger bones use HandsClosed when grip is active
		int32 EffAnimIdx = AnimIdx;
		float EffTimeMs = TimeMs;

		bool bIsRightFinger = BoneID >= 8 && BoneID <= 12;
		bool bIsLeftFinger = BoneID >= 13 && BoneID <= 17;
		bool bUseClosedHand = (bIsRightFinger && bCloseR) || (bIsLeftFinger && bCloseL);

		if (bUseClosedHand)
		{
			EffAnimIdx = HandsClosedAnimIndex;
			EffTimeMs = 0.f;
		}

		const M2Track* TransTrack = GetBoneTrack(Loader, SkelLoader, CurrentAnimSource, Idx, 0);
		const M2Track* RotTrack = GetBoneTrack(Loader, SkelLoader, CurrentAnimSource, Idx, 2);
		const M2Track* ScaleTrack = GetBoneTrack(Loader, SkelLoader, CurrentAnimSource, Idx, 1);

		bool bHasTrans = TransTrack && EffAnimIdx >= 0 && EffAnimIdx < static_cast<int32>(TransTrack->timestamps.size()) && !TransTrack->timestamps[EffAnimIdx].empty();
		bool bHasRot = RotTrack && EffAnimIdx >= 0 && EffAnimIdx < static_cast<int32>(RotTrack->timestamps.size()) && !RotTrack->timestamps[EffAnimIdx].empty();
		bool bHasScale = ScaleTrack && EffAnimIdx >= 0 && EffAnimIdx < static_cast<int32>(ScaleTrack->timestamps.size()) && !ScaleTrack->timestamps[EffAnimIdx].empty();
		bool bHasScaleFallback = !bHasScale && EffAnimIdx != 0 && ScaleTrack && !ScaleTrack->timestamps.empty() && !ScaleTrack->timestamps[0].empty();

		FMatrix LocalMat = FMatrix::Identity;

		if (bHasTrans || bHasRot || bHasScale || bHasScaleFallback)
		{
			FMatrix Result = FMatrix::Identity;

			FMatrix PivotMat = FMatrix::Identity;
			PivotMat.M[3][0] = Px; PivotMat.M[3][1] = Py; PivotMat.M[3][2] = Pz;
			Result = Result * PivotMat;

			if (bHasTrans)
			{
				FVector T = SampleVec3(Idx, 0, EffAnimIdx, EffTimeMs, FVector::ZeroVector);
				FMatrix TransMat = FMatrix::Identity;
				TransMat.M[3][0] = T.X; TransMat.M[3][1] = T.Y; TransMat.M[3][2] = T.Z;
				Result = Result * TransMat;
			}

			if (bHasRot)
			{
				FQuat Q = SampleQuat(Idx, EffAnimIdx, EffTimeMs);
				Result = Result * FQuatRotationMatrix(Q);
			}

			if (bHasScale || bHasScaleFallback)
			{
				int32 ScaleAnimIdx = bHasScale ? EffAnimIdx : 0;
				float ScaleTime = bHasScale ? EffTimeMs : 0.f;
				FVector S = SampleVec3(Idx, 1, ScaleAnimIdx, ScaleTime, FVector::OneVector);
				FMatrix ScaleMat = FMatrix::Identity;
				ScaleMat.M[0][0] = S.X; ScaleMat.M[1][1] = S.Y; ScaleMat.M[2][2] = S.Z;
				Result = Result * ScaleMat;
			}

			FMatrix NegPivotMat = FMatrix::Identity;
			NegPivotMat.M[3][0] = -Px; NegPivotMat.M[3][1] = -Py; NegPivotMat.M[3][2] = -Pz;
			Result = Result * NegPivotMat;

			LocalMat = Result;
		}

		if (ParentIdx >= 0 && ParentIdx < BoneCount)
			ComponentMatrices[Idx] = ComponentMatrices[ParentIdx] * LocalMat;
		else
			ComponentMatrices[Idx] = LocalMat;

		BoneCalculated[Idx] = true;
	};

	for (int32 i = 0; i < BoneCount; ++i)
		CalcBone(i);

	// Convert component-space WowLib matrices to UE bone-local FTransforms
	for (int32 i = 0; i < BoneCount; ++i)
	{
		FMatrix WL_Local = FMatrix::Identity;
		int32 ParentIdx = -1;
		if (SkelLoader && i < static_cast<int32>(SkelLoader->bones.size()))
			ParentIdx = SkelLoader->bones[i].parentBone;
		else if (Loader && i < static_cast<int32>(Loader->bones.size()))
			ParentIdx = Loader->bones[i].parentBone;

		if (ParentIdx >= 0 && ParentIdx < BoneCount)
			WL_Local = ComponentMatrices[ParentIdx].Inverse() * ComponentMatrices[i];
		else
			WL_Local = ComponentMatrices[i];

		FVector WL_Trans(WL_Local.M[3][0], WL_Local.M[3][1], WL_Local.M[3][2]);

		FVector WL_ScaleX(WL_Local.M[0][0], WL_Local.M[0][1], WL_Local.M[0][2]);
		FVector WL_ScaleY(WL_Local.M[1][0], WL_Local.M[1][1], WL_Local.M[1][2]);
		FVector WL_ScaleZ(WL_Local.M[2][0], WL_Local.M[2][1], WL_Local.M[2][2]);
		float Sx = WL_ScaleX.Size(), Sy = WL_ScaleY.Size(), Sz = WL_ScaleZ.Size();

		FMatrix RotMat = WL_Local;
		if (Sx > SMALL_NUMBER) { RotMat.M[0][0] /= Sx; RotMat.M[0][1] /= Sx; RotMat.M[0][2] /= Sx; }
		if (Sy > SMALL_NUMBER) { RotMat.M[1][0] /= Sy; RotMat.M[1][1] /= Sy; RotMat.M[1][2] /= Sy; }
		if (Sz > SMALL_NUMBER) { RotMat.M[2][0] /= Sz; RotMat.M[2][1] /= Sz; RotMat.M[2][2] /= Sz; }
		RotMat.M[3][0] = 0; RotMat.M[3][1] = 0; RotMat.M[3][2] = 0;
		FQuat WL_Rot = FQuat(RotMat);

		FVector UE_Trans = WowLibToUE_Translation(WL_Trans.X, WL_Trans.Y, WL_Trans.Z);
		FQuat UE_Rot = WowLibToUE_Rotation(WL_Rot.X, WL_Rot.Y, WL_Rot.Z, WL_Rot.W);
		FVector UE_Scale = WowLibToUE_Scale(Sx, Sy, Sz);

		BoneLocalTransforms[i] = FTransform(UE_Rot, UE_Trans, UE_Scale);
	}
}
