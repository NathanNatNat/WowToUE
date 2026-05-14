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

	// Cache parent indices and bone IDs from structural skeleton
	BoneParentIndices.SetNum(BoneCount);
	BoneIDs.SetNum(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i)
	{
		if (SkelLoader && i < static_cast<int32>(SkelLoader->bones.size()))
		{
			BoneParentIndices[i] = SkelLoader->bones[i].parentBone;
			BoneIDs[i] = SkelLoader->bones[i].boneID;
		}
		else if (Loader && i < static_cast<int32>(Loader->bones.size()))
		{
			BoneParentIndices[i] = Loader->bones[i].parentBone;
			BoneIDs[i] = Loader->bones[i].boneID;
		}
		else
		{
			BoneParentIndices[i] = -1;
			BoneIDs[i] = -1;
		}
	}

	BoneLocalTransforms.SetNum(BoneCount);
	BoneCalculated.SetNum(BoneCount);

	// Initialize global sequence times
	const auto& GlobalLoops = (SkelLoader) ? SkelLoader->globalLoops : Loader->globalLoops;
	GlobalSeqTimes.SetNumZeroed(GlobalLoops.size());

	StopAnimation();
}

void FWowM2Animator::SetUEPivots(const TArray<FVector>& InPivots)
{
	UEPivots = InPivots;
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
	CalcSubmeshAlphas();
	CalcTexTransforms();
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
		CalcSubmeshAlphas();
		CalcTexTransforms();

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
	CalcSubmeshAlphas();
	CalcTexTransforms();
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
	CalcSubmeshAlphas();
	CalcTexTransforms();
}

void FWowM2Animator::StepFrame(int32 Delta)
{
	int32 Frame = GetCurrentFrame() + Delta;
	int32 Count = GetFrameCount();
	if (Frame < 0) Frame = Count - 1;
	else if (Frame >= Count) Frame = 0;
	SetFrame(Frame);
}

// Direct WoW→UE: negate Y to flip handedness (det=-1). No WebGL intermediate.
static FVector WowToUE_Translation(float x, float y, float z)
{
	return FVector(x * 100.0, -y * 100.0, z * 100.0);
}

static FQuat WowToUE_Rotation(float qx, float qy, float qz, float qw)
{
	return FQuat(-qx, qy, -qz, qw);
}

static FVector WowToUE_Scale(float sx, float sy, float sz)
{
	return FVector(sx, sy, sz);
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

	// Step interpolation: return exact keyframe value without lerp
	if (Track->interpolation == 0)
		return GetVec3(Values[Frame], Default);

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

	// Step interpolation: return exact keyframe value without slerp
	if (Track.interpolation == 0)
		return GetQuat(Values[Frame]);

	float T0 = GetTimestampMs(Timestamps[Frame]);
	float T1 = GetTimestampMs(Timestamps[Frame + 1]);
	float Dt = T1 - T0;
	float Alpha = Dt > 0.f ? FMath::Min((TimeMs - T0) / Dt, 1.f) : 0.f;

	FQuat Q0 = GetQuat(Values[Frame]);
	FQuat Q1 = GetQuat(Values[Frame + 1]);
	return FQuat::Slerp(Q0, Q1, Alpha);
}

void FWowM2Animator::SetSubmeshInfo(const TArray<int32>& InColorIndices, const TArray<int32>& InTexWeightIndices,
	const TArray<int32>& InTexWeightIndices1, const TArray<int32>& InTexWeightIndices2,
	const TArray<uint8>& InTUFlags)
{
	SubmeshColorIndices = InColorIndices;
	SubmeshTexWeightIndices = InTexWeightIndices;
	SubmeshTexWeightIndices1 = InTexWeightIndices1;
	SubmeshTexWeightIndices2 = InTexWeightIndices2;
	SubmeshTUFlags = InTUFlags;
	int32 Count = InColorIndices.Num();
	SubmeshAlphas.SetNumUninitialized(Count);
	SubmeshAnimData.SetNum(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		SubmeshAlphas[i] = 1.f;
		SubmeshAnimData[i] = FSubmeshAnimData();
	}
}

static float SampleTrackFloat(const M2Track& Track, int32 AnimIdx, float TimeMs, float Default)
{
	if (AnimIdx < 0 || AnimIdx >= static_cast<int32>(Track.timestamps.size()))
		return Default;

	const auto& Timestamps = Track.timestamps[AnimIdx];
	const auto& Values = Track.values[AnimIdx];
	if (Timestamps.empty() || Values.empty())
		return Default;

	auto GetFloat = [](const M2Value& V, float Def) -> float {
		if (auto* P = std::get_if<std::vector<float>>(&V))
			return P->empty() ? Def : (*P)[0];
		if (auto* P = std::get_if<uint32_t>(&V))
			return static_cast<float>(*P);
		if (auto* P = std::get_if<int16_t>(&V))
			return static_cast<float>(*P);
		return Def;
	};

	if (Timestamps.size() == 1 || TimeMs <= GetTimestampMs(Timestamps[0]))
		return GetFloat(Values[0], Default);

	if (TimeMs >= GetTimestampMs(Timestamps.back()))
		return GetFloat(Values.back(), Default);

	int32 Frame = FindKeyframe(Timestamps, TimeMs);
	if (Track.interpolation == 0)
		return GetFloat(Values[Frame], Default);

	float T0 = GetTimestampMs(Timestamps[Frame]);
	float T1 = GetTimestampMs(Timestamps[Frame + 1]);
	float Dt = T1 - T0;
	float Alpha = Dt > 0.f ? FMath::Min((TimeMs - T0) / Dt, 1.f) : 0.f;

	float V0 = GetFloat(Values[Frame], Default);
	float V1 = GetFloat(Values[Frame + 1], Default);
	return FMath::Lerp(V0, V1, Alpha);
}

static float SampleTrackWithGlobalSeq(const M2Track& Track, int32 AnimIdx, float TimeMs, float Default,
	const TArray<float>& GlobalSeqTimes)
{
	int32 EffAnimIdx = AnimIdx;
	float EffTimeMs = TimeMs;

	if (Track.globalSeq > 0)
	{
		EffAnimIdx = 0;
		int32 GSIdx = Track.globalSeq - 1;
		EffTimeMs = GlobalSeqTimes.IsValidIndex(GSIdx) ? GlobalSeqTimes[GSIdx] : 0.f;
	}

	return SampleTrackFloat(Track, EffAnimIdx, EffTimeMs, Default);
}

static FVector SampleVec3TrackWithGlobalSeq(const M2Track& Track, int32 AnimIdx, float TimeMs, const FVector& Default,
	const TArray<float>& GlobalSeqTimes)
{
	int32 EffAnimIdx = AnimIdx;
	float EffTimeMs = TimeMs;

	if (Track.globalSeq > 0)
	{
		EffAnimIdx = 0;
		int32 GSIdx = Track.globalSeq - 1;
		EffTimeMs = GlobalSeqTimes.IsValidIndex(GSIdx) ? GlobalSeqTimes[GSIdx] : 0.f;
	}

	if (EffAnimIdx < 0 || EffAnimIdx >= static_cast<int32>(Track.timestamps.size()))
		return Default;

	const auto& Timestamps = Track.timestamps[EffAnimIdx];
	const auto& Values = Track.values[EffAnimIdx];
	if (Timestamps.empty() || Values.empty()) return Default;

	if (Timestamps.size() == 1 || EffTimeMs <= GetTimestampMs(Timestamps[0]))
		return GetVec3(Values[0], Default);
	if (EffTimeMs >= GetTimestampMs(Timestamps.back()))
		return GetVec3(Values.back(), Default);

	int32 Frame = FindKeyframe(Timestamps, EffTimeMs);
	if (Track.interpolation == 0)
		return GetVec3(Values[Frame], Default);

	float T0 = GetTimestampMs(Timestamps[Frame]);
	float T1 = GetTimestampMs(Timestamps[Frame + 1]);
	float Dt = T1 - T0;
	float Alpha = Dt > 0.f ? FMath::Min((EffTimeMs - T0) / Dt, 1.f) : 0.f;
	return FMath::Lerp(GetVec3(Values[Frame], Default), GetVec3(Values[Frame + 1], Default), Alpha);
}

static FQuat SampleQuatTrackWithGlobalSeq(const M2Track& Track, int32 AnimIdx, float TimeMs,
	const TArray<float>& GlobalSeqTimes)
{
	int32 EffAnimIdx = AnimIdx;
	float EffTimeMs = TimeMs;

	if (Track.globalSeq > 0)
	{
		EffAnimIdx = 0;
		int32 GSIdx = Track.globalSeq - 1;
		EffTimeMs = GlobalSeqTimes.IsValidIndex(GSIdx) ? GlobalSeqTimes[GSIdx] : 0.f;
	}

	if (EffAnimIdx < 0 || EffAnimIdx >= static_cast<int32>(Track.timestamps.size()))
		return FQuat::Identity;

	const auto& Timestamps = Track.timestamps[EffAnimIdx];
	const auto& Values = Track.values[EffAnimIdx];
	if (Timestamps.empty() || Values.empty()) return FQuat::Identity;

	if (Timestamps.size() == 1 || EffTimeMs <= GetTimestampMs(Timestamps[0]))
		return GetQuat(Values[0]);
	if (EffTimeMs >= GetTimestampMs(Timestamps.back()))
		return GetQuat(Values.back());

	int32 Frame = FindKeyframe(Timestamps, EffTimeMs);
	if (Track.interpolation == 0)
		return GetQuat(Values[Frame]);

	float T0 = GetTimestampMs(Timestamps[Frame]);
	float T1 = GetTimestampMs(Timestamps[Frame + 1]);
	float Dt = T1 - T0;
	float Alpha = Dt > 0.f ? FMath::Min((EffTimeMs - T0) / Dt, 1.f) : 0.f;
	return FQuat::Slerp(GetQuat(Values[Frame]), GetQuat(Values[Frame + 1]), Alpha);
}

void FWowM2Animator::CalcSubmeshAlphas()
{
	if (!Loader || SubmeshAlphas.Num() == 0) return;

	const float TimeMs = AnimationTime * 1000.f;
	const int32 AnimIdx = CurrentAnimIndex;

	for (int32 i = 0; i < SubmeshAlphas.Num(); ++i)
	{
		float ColorAlpha = 1.f;
		FLinearColor ColorRGB = FLinearColor::White;

		int32 ColorIdx = SubmeshColorIndices.IsValidIndex(i) ? SubmeshColorIndices[i] : -1;
		if (ColorIdx >= 0 && ColorIdx < static_cast<int32>(Loader->colors.size()))
		{
			float A = SampleTrackWithGlobalSeq(Loader->colors[ColorIdx].alpha, AnimIdx, TimeMs, 32767.f, GlobalSeqTimes);
			ColorAlpha = A / 32768.f;

			const M2Track& ColorTrack = Loader->colors[ColorIdx].color;
			FVector C = SampleVec3TrackWithGlobalSeq(ColorTrack, AnimIdx, TimeMs, FVector::OneVector, GlobalSeqTimes);
			ColorRGB = FLinearColor(C.X, C.Y, C.Z);
		}

		auto SampleWeight = [&](int32 WeightIdx) -> float {
			if (WeightIdx < 0 || WeightIdx >= static_cast<int32>(Loader->textureWeights.size()))
				return 1.f;
			float W = SampleTrackWithGlobalSeq(Loader->textureWeights[WeightIdx], AnimIdx, TimeMs, 32767.f, GlobalSeqTimes);
			return W / 32768.f;
		};

		float TexWeight0 = SampleWeight(SubmeshTexWeightIndices.IsValidIndex(i) ? SubmeshTexWeightIndices[i] : -1);
		float TexWeight1 = SampleWeight(SubmeshTexWeightIndices1.IsValidIndex(i) ? SubmeshTexWeightIndices1[i] : -1);
		float TexWeight2 = SampleWeight(SubmeshTexWeightIndices2.IsValidIndex(i) ? SubmeshTexWeightIndices2[i] : -1);

		bool bApplyWeight = !(SubmeshTUFlags.IsValidIndex(i) && (SubmeshTUFlags[i] & 0x40));

		float FinalAlpha = ColorAlpha;
		if (bApplyWeight)
			FinalAlpha *= TexWeight0;

		SubmeshAlphas[i] = FinalAlpha;

		if (SubmeshAnimData.IsValidIndex(i))
		{
			SubmeshAnimData[i].Color = ColorRGB;
			SubmeshAnimData[i].Alpha = FinalAlpha;
			SubmeshAnimData[i].TexSampleAlpha = FVector(TexWeight0, TexWeight1, TexWeight2);
			SubmeshAnimData[i].bApplyWeight = bApplyWeight;
		}
	}
}

void FWowM2Animator::CalcTexTransforms()
{
	if (!Loader) return;

	const float TimeMs = AnimationTime * 1000.f;
	const int32 AnimIdx = CurrentAnimIndex;
	const int32 NumTransforms = static_cast<int32>(Loader->textureTransforms.size());

	TexTransformMatrices.SetNum(NumTransforms);

	for (int32 i = 0; i < NumTransforms; ++i)
	{
		const auto& TT = Loader->textureTransforms[i];

		// Composition order matches WebWowViewerCpp animationManager.cpp:
		// Identity → T(pivot) → Rotate → T(-pivot) → T(pivot) → Scale → T(-pivot) → Translate
		// Pivot = (0.5, 0.5) in UV space

		float M[16];
		FMemory::Memzero(M, sizeof(M));
		M[0] = M[5] = M[10] = M[15] = 1.f;

		auto Mat4Mul = [](float* out, const float* a, const float* b) {
			float tmp[16];
			for (int c = 0; c < 4; ++c)
				for (int r = 0; r < 4; ++r)
					tmp[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1]
						+ a[2 * 4 + r] * b[c * 4 + 2] + a[3 * 4 + r] * b[c * 4 + 3];
			FMemory::Memcpy(out, tmp, sizeof(tmp));
		};

		auto Mat4Trans = [](float* m, float x, float y, float z) {
			FMemory::Memzero(m, 16 * sizeof(float));
			m[0] = m[5] = m[10] = m[15] = 1.f;
			m[12] = x; m[13] = y; m[14] = z;
		};

		auto Mat4Scale = [](float* m, float x, float y, float z) {
			FMemory::Memzero(m, 16 * sizeof(float));
			m[0] = x; m[5] = y; m[10] = z; m[15] = 1.f;
		};

		auto Mat4Quat = [](float* m, float qx, float qy, float qz, float qw) {
			float x2 = qx + qx, y2 = qy + qy, z2 = qz + qz;
			float xx = qx * x2, xy = qx * y2, xz = qx * z2;
			float yy = qy * y2, yz = qy * z2, zz = qz * z2;
			float wx = qw * x2, wy = qw * y2, wz = qw * z2;
			m[0] = 1-(yy+zz); m[1] = xy+wz;     m[2] = xz-wy;     m[3] = 0;
			m[4] = xy-wz;     m[5] = 1-(xx+zz); m[6] = yz+wx;     m[7] = 0;
			m[8] = xz+wy;     m[9] = yz-wx;     m[10] = 1-(xx+yy); m[11] = 0;
			m[12] = 0;         m[13] = 0;         m[14] = 0;         m[15] = 1;
		};

		float tmp[16], p[16], np[16];
		const float Px = 0.5f, Py = 0.5f;

		bool bHasRot = !TT.rotation.timestamps.empty();
		bool bHasScale = !TT.scaling.timestamps.empty();
		bool bHasTrans = !TT.translation.timestamps.empty();

		if (bHasRot)
		{
			Mat4Trans(p, Px, Py, 0.f);
			Mat4Mul(tmp, M, p);
			FMemory::Memcpy(M, tmp, sizeof(M));

			FQuat Q = SampleQuatTrackWithGlobalSeq(TT.rotation, AnimIdx, TimeMs, GlobalSeqTimes);
			float r[16];
			Mat4Quat(r, Q.X, Q.Y, Q.Z, Q.W);
			Mat4Mul(tmp, M, r);
			FMemory::Memcpy(M, tmp, sizeof(M));

			Mat4Trans(np, -Px, -Py, 0.f);
			Mat4Mul(tmp, M, np);
			FMemory::Memcpy(M, tmp, sizeof(M));
		}

		if (bHasScale)
		{
			Mat4Trans(p, Px, Py, 0.f);
			Mat4Mul(tmp, M, p);
			FMemory::Memcpy(M, tmp, sizeof(M));

			FVector S = SampleVec3TrackWithGlobalSeq(TT.scaling, AnimIdx, TimeMs, FVector::OneVector, GlobalSeqTimes);
			float s[16];
			Mat4Scale(s, S.X, S.Y, S.Z);
			Mat4Mul(tmp, M, s);
			FMemory::Memcpy(M, tmp, sizeof(M));

			Mat4Trans(np, -Px, -Py, 0.f);
			Mat4Mul(tmp, M, np);
			FMemory::Memcpy(M, tmp, sizeof(M));
		}

		if (bHasTrans)
		{
			FVector T = SampleVec3TrackWithGlobalSeq(TT.translation, AnimIdx, TimeMs, FVector::ZeroVector, GlobalSeqTimes);
			float t[16];
			Mat4Trans(t, T.X, T.Y, T.Z);
			Mat4Mul(tmp, M, t);
			FMemory::Memcpy(M, tmp, sizeof(M));
		}

		// Extract the two rows needed for 2D UV transform:
		// result.x = M[0]*u + M[4]*v + M[8]*0 + M[12]*1 (column-major)
		// result.y = M[1]*u + M[5]*v + M[9]*0 + M[13]*1
		TexTransformMatrices[i].Row0 = FVector4(M[0], M[4], M[8], M[12]);
		TexTransformMatrices[i].Row1 = FVector4(M[1], M[5], M[9], M[13]);
	}
}

void FWowM2Animator::CalcAllBones()
{
	if (BoneCount == 0 || UEPivots.Num() < BoneCount) return;

	const float TimeMs = AnimationTime * 1000.f;
	const int32 AnimIdx = CurrentAnimIndex;

	BoneLocalTransforms.SetNum(BoneCount);
	BoneCalculated.SetNum(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i) BoneCalculated[i] = false;

	// Calculate bone matrices EXACTLY like wow.export (WowLib space, column-major)
	// Using raw float[16] arrays to avoid any UE FMatrix convention confusion
	// Then convert final matrices to UE FTransform via similarity transform

	// Column-major 4x4 matrix helpers (matching wow.export's mat4_multiply)
	auto Mat4Identity = [](float* m) {
		FMemory::Memzero(m, 16 * sizeof(float));
		m[0] = m[5] = m[10] = m[15] = 1.f;
	};

	auto Mat4Multiply = [](float* out, const float* a, const float* b) {
		for (int c = 0; c < 4; ++c) {
			for (int r = 0; r < 4; ++r) {
				out[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0]
					+ a[1 * 4 + r] * b[c * 4 + 1]
					+ a[2 * 4 + r] * b[c * 4 + 2]
					+ a[3 * 4 + r] * b[c * 4 + 3];
			}
		}
	};

	auto Mat4FromTranslation = [](float* m, float x, float y, float z) {
		FMemory::Memzero(m, 16 * sizeof(float));
		m[0] = m[5] = m[10] = m[15] = 1.f;
		m[12] = x; m[13] = y; m[14] = z;
	};

	auto Mat4FromScale = [](float* m, float x, float y, float z) {
		FMemory::Memzero(m, 16 * sizeof(float));
		m[0] = x; m[5] = y; m[10] = z; m[15] = 1.f;
	};

	auto Mat4FromQuat = [](float* m, float qx, float qy, float qz, float qw) {
		float x2 = qx + qx, y2 = qy + qy, z2 = qz + qz;
		float xx = qx * x2, xy = qx * y2, xz = qx * z2;
		float yy = qy * y2, yz = qy * z2, zz = qz * z2;
		float wx = qw * x2, wy = qw * y2, wz = qw * z2;
		m[0] = 1 - (yy + zz); m[1] = xy + wz;       m[2] = xz - wy;       m[3] = 0;
		m[4] = xy - wz;       m[5] = 1 - (xx + zz); m[6] = yz + wx;       m[7] = 0;
		m[8] = xz + wy;       m[9] = yz - wx;       m[10] = 1 - (xx + yy); m[11] = 0;
		m[12] = 0;             m[13] = 0;             m[14] = 0;             m[15] = 1;
	};

	auto Mat4Copy = [](float* dst, const float* src) { FMemory::Memcpy(dst, src, 16 * sizeof(float)); };

	TArray<float> BoneMatrices;
	BoneMatrices.SetNumZeroed(BoneCount * 16);

	const bool bCloseR = bCloseRightHand && HandsClosedAnimIndex >= 0;
	const bool bCloseL = bCloseLeftHand && HandsClosedAnimIndex >= 0;

	float local_mat[16], temp[16], pivot_mat[16], trans_mat[16], rot_mat[16], scale_mat[16], neg_pivot_mat[16];

	TFunction<void(int32)> CalcBone = [&](int32 Idx)
	{
		if (Idx < 0 || Idx >= BoneCount || BoneCalculated[Idx])
			return;

		int32 ParentIdx = BoneParentIndices[Idx];
		int32 BoneID = BoneIDs[Idx];

		if (ParentIdx >= 0 && ParentIdx < BoneCount)
			CalcBone(ParentIdx);

		// Get WowLib-space pivot from structural skeleton
		float Px = 0, Py = 0, Pz = 0;
		if (SkelLoader && Idx < static_cast<int32>(SkelLoader->bones.size()))
		{
			const auto& B = SkelLoader->bones[Idx];
			if (B.pivot.size() >= 3) { Px = B.pivot[0]; Py = B.pivot[1]; Pz = B.pivot[2]; }
		}
		else if (Loader && Idx < static_cast<int32>(Loader->bones.size()))
		{
			const auto& B = Loader->bones[Idx];
			if (B.pivot.size() >= 3) { Px = B.pivot[0]; Py = B.pivot[1]; Pz = B.pivot[2]; }
		}

		int32 EffAnimIdx = AnimIdx;
		float EffTimeMs = TimeMs;

		if ((BoneID >= 8 && BoneID <= 12 && bCloseR) || (BoneID >= 13 && BoneID <= 17 && bCloseL))
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

		Mat4Identity(local_mat);

		if (bHasTrans || bHasRot || bHasScale || bHasScaleFallback)
		{
			// wow.export column-major: local = I * T(pivot) * T(trans) * R(rot) * S(scale) * T(-pivot)
			Mat4FromTranslation(pivot_mat, Px, Py, Pz);
			Mat4Multiply(temp, local_mat, pivot_mat);
			Mat4Copy(local_mat, temp);

			if (bHasTrans)
			{
				FVector T = SampleVec3(Idx, 0, EffAnimIdx, EffTimeMs, FVector::ZeroVector);
				Mat4FromTranslation(trans_mat, T.X, T.Y, T.Z);
				Mat4Multiply(temp, local_mat, trans_mat);
				Mat4Copy(local_mat, temp);
			}

			if (bHasRot)
			{
				FQuat Q = SampleQuat(Idx, EffAnimIdx, EffTimeMs);
				Mat4FromQuat(rot_mat, Q.X, Q.Y, Q.Z, Q.W);
				Mat4Multiply(temp, local_mat, rot_mat);
				Mat4Copy(local_mat, temp);
			}

			if (bHasScale || bHasScaleFallback)
			{
				int32 ScaleAnimIdx = bHasScale ? EffAnimIdx : 0;
				float ScaleTime = bHasScale ? EffTimeMs : 0.f;
				FVector S = SampleVec3(Idx, 1, ScaleAnimIdx, ScaleTime, FVector::OneVector);
				Mat4FromScale(scale_mat, S.X, S.Y, S.Z);
				Mat4Multiply(temp, local_mat, scale_mat);
				Mat4Copy(local_mat, temp);
			}

			Mat4FromTranslation(neg_pivot_mat, -Px, -Py, -Pz);
			Mat4Multiply(temp, local_mat, neg_pivot_mat);
			Mat4Copy(local_mat, temp);
		}

		// Parent chain: component = parent * local (column-major)
		float* CompMat = &BoneMatrices[Idx * 16];
		if (ParentIdx >= 0 && ParentIdx < BoneCount)
		{
			const float* ParentMat = &BoneMatrices[ParentIdx * 16];
			Mat4Multiply(CompMat, ParentMat, local_mat);
		}
		else
		{
			Mat4Copy(CompMat, local_mat);
		}

		// Output bone-local animated transform for UE's BoneSpaceTransforms
		// RefBonePose has pivot offsets. BoneSpaceTransforms = pivot offset + animation.
		// At rest (no animation data), output the pivot offset (= ref pose).
		// With animation, output: rotation + (pivot offset + anim translation) + scale.
		{
			FVector PivotOffset = (Idx < UEPivots.Num()) ? UEPivots[Idx] : FVector::ZeroVector;
			FVector ParentPivot = (ParentIdx >= 0 && ParentIdx < UEPivots.Num()) ? UEPivots[ParentIdx] : FVector::ZeroVector;
			FVector LocalPivotOffset = PivotOffset - ParentPivot;

			if (bHasTrans || bHasRot || bHasScale || bHasScaleFallback)
			{
				FVector WL_T = bHasTrans ? SampleVec3(Idx, 0, EffAnimIdx, EffTimeMs, FVector::ZeroVector) : FVector::ZeroVector;
				FQuat WL_Q = bHasRot ? SampleQuat(Idx, EffAnimIdx, EffTimeMs) : FQuat(0,0,0,1);
				FVector WL_S = FVector::OneVector;
				if (bHasScale || bHasScaleFallback)
				{
					int32 ScaleAnimIdx = bHasScale ? EffAnimIdx : 0;
					float ScaleTime = bHasScale ? EffTimeMs : 0.f;
					WL_S = SampleVec3(Idx, 1, ScaleAnimIdx, ScaleTime, FVector::OneVector);
				}

				FVector UE_Trans = LocalPivotOffset + WowToUE_Translation(WL_T.X, WL_T.Y, WL_T.Z);
				FQuat UE_Rot = WowToUE_Rotation(WL_Q.X, WL_Q.Y, WL_Q.Z, WL_Q.W);
				FVector UE_Scale = WowToUE_Scale(WL_S.X, WL_S.Y, WL_S.Z);

				BoneLocalTransforms[Idx] = FTransform(UE_Rot, UE_Trans, UE_Scale);
			}
			else
			{
				// No animation — use rest pose (pivot offset only)
				BoneLocalTransforms[Idx] = FTransform(FQuat::Identity, LocalPivotOffset);
			}
		}

		BoneCalculated[Idx] = true;
	};

	for (int32 i = 0; i < BoneCount; ++i)
		CalcBone(i);

	// Debug: log raw WowLib bone matrices for comparison with wow.export
	static bool bLoggedWL = false;
	if (!bLoggedWL && AnimIdx >= 0)
	{
		for (int32 i = 0; i < FMath::Min(5, BoneCount); ++i)
		{
			const float* M = &BoneMatrices[i * 16];
			UE_LOG(LogTemp, Log, TEXT("WL Bone[%d]: pos=[%.4f,%.4f,%.4f] rot00=%.4f rot01=%.4f rot10=%.4f rot11=%.4f"),
				i, M[12], M[13], M[14], M[0], M[1], M[4], M[5]);
		}
		bLoggedWL = true;
	}
}
