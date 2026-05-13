#include "WowM2Animator.h"
#include "3D/loaders/M2Loader.h"
#include "3D/loaders/M2Generics.h"

void FWowM2Animator::Initialize(M2Loader* InLoader)
{
	Loader = InLoader;
	if (!Loader) return;

	HandsClosedAnimIndex = -1;
	for (size_t i = 0; i < Loader->animations.size(); ++i)
	{
		if (Loader->animations[i].id == 15)
		{
			HandsClosedAnimIndex = static_cast<int32>(i);
			break;
		}
	}

	BoneLocalTransforms.SetNum(Loader->bones.size());
	BoneCalculated.SetNum(Loader->bones.size());

	StopAnimation();
}

void FWowM2Animator::PlayAnimation(int32 AnimIndex)
{
	if (!Loader) return;

	if (AnimIndex >= 0 && AnimIndex < static_cast<int32>(Loader->animations.size()))
	{
		auto& Anim = Loader->animations[AnimIndex];
		int32 ResolvedIndex = AnimIndex;

		// Resolve alias animations (flag 0x40)
		while (Anim.flags & 0x40)
		{
			ResolvedIndex = Anim.aliasNext;
			if (ResolvedIndex < 0 || ResolvedIndex >= static_cast<int32>(Loader->animations.size()))
				break;
			Anim = Loader->animations[ResolvedIndex];
		}

		// Load external .anim data if needed
		Loader->loadAnimsForIndex(ResolvedIndex).get();
	}

	CurrentAnimIndex = AnimIndex;
	AnimationTime = 0.f;
	CalcAllBones();
}

void FWowM2Animator::StopAnimation()
{
	CurrentAnimIndex = 0;
	AnimationTime = 0.f;
	CalcAllBones();
}

void FWowM2Animator::Update(float DeltaTimeSeconds)
{
	if (!Loader || CurrentAnimIndex < 0) return;

	if (!bPaused)
	{
		AnimationTime += DeltaTimeSeconds;
		float Duration = GetDuration();
		if (Duration > 0.f)
			AnimationTime = FMath::Fmod(AnimationTime, Duration);
	}

	CalcAllBones();
}

float FWowM2Animator::GetDuration() const
{
	if (!Loader || CurrentAnimIndex < 0 || CurrentAnimIndex >= static_cast<int32>(Loader->animations.size()))
		return 0.f;
	return Loader->animations[CurrentAnimIndex].duration / 1000.f;
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
	{
		if (P->size() >= 3) return FVector((*P)[0], (*P)[1], (*P)[2]);
	}
	return Def;
}

static FQuat GetQuat(const M2Value& V)
{
	if (auto* P = std::get_if<std::vector<float>>(&V))
	{
		if (P->size() >= 4) return FQuat((*P)[0], (*P)[1], (*P)[2], (*P)[3]);
	}
	return FQuat::Identity;
}

FVector FWowM2Animator::SampleVec3(int32 BoneIndex, int32 TrackType, int32 AnimIdx, float TimeMs, const FVector& Default)
{
	if (!Loader || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Loader->bones.size()))
		return Default;

	const M2Track* Track = nullptr;
	if (TrackType == 0) Track = &Loader->bones[BoneIndex].translation;
	else Track = &Loader->bones[BoneIndex].scale;

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
	if (!Loader || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Loader->bones.size()))
		return FQuat::Identity;

	const auto& Track = Loader->bones[BoneIndex].rotation;

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
	if (!Loader || Loader->bones.empty()) return;

	const int32 BoneCount = static_cast<int32>(Loader->bones.size());
	const float TimeMs = AnimationTime * 1000.f;
	const int32 AnimIdx = CurrentAnimIndex;

	BoneLocalTransforms.SetNum(BoneCount);
	BoneCalculated.SetNum(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i) BoneCalculated[i] = false;

	// Component-space matrices (WowLib space, then converted to UE)
	TArray<FMatrix> ComponentMatrices;
	ComponentMatrices.SetNum(BoneCount);

	TFunction<void(int32)> CalcBone = [&](int32 Idx)
	{
		if (Idx < 0 || Idx >= BoneCount || BoneCalculated[Idx])
			return;

		const auto& Bone = Loader->bones[Idx];
		int32 ParentIdx = Bone.parentBone;

		if (ParentIdx >= 0 && ParentIdx < BoneCount)
			CalcBone(ParentIdx);

		float Px = Bone.pivot[0], Py = Bone.pivot[1], Pz = Bone.pivot[2];

		int32 EffAnimIdx = AnimIdx;
		float EffTimeMs = TimeMs;

		// Hand grip: finger bones use HandsClosed animation
		if (HandsClosedAnimIndex >= 0)
		{
			bool bRightFinger = Bone.boneID >= 8 && Bone.boneID <= 12;
			bool bLeftFinger = Bone.boneID >= 13 && Bone.boneID <= 17;
			if (bRightFinger || bLeftFinger)
			{
				EffAnimIdx = HandsClosedAnimIndex;
				EffTimeMs = 0.f;
			}
		}

		bool bHasTrans = EffAnimIdx >= 0 && EffAnimIdx < static_cast<int32>(Bone.translation.timestamps.size()) && !Bone.translation.timestamps[EffAnimIdx].empty();
		bool bHasRot = EffAnimIdx >= 0 && EffAnimIdx < static_cast<int32>(Bone.rotation.timestamps.size()) && !Bone.rotation.timestamps[EffAnimIdx].empty();
		bool bHasScale = EffAnimIdx >= 0 && EffAnimIdx < static_cast<int32>(Bone.scale.timestamps.size()) && !Bone.scale.timestamps[EffAnimIdx].empty();
		bool bHasScaleFallback = !bHasScale && EffAnimIdx != 0 && !Bone.scale.timestamps.empty() && !Bone.scale.timestamps[0].empty();

		FMatrix LocalMat = FMatrix::Identity;

		if (bHasTrans || bHasRot || bHasScale || bHasScaleFallback)
		{
			// wow.export column-major: T(pivot) * T(trans) * R(rot) * S(scale) * T(-pivot)
			// This is applied as: result = Identity * Pivot * Trans * Rot * Scale * NegPivot
			// Using column-major mat4_multiply(out, a, b) = a * b

			// Build in column-major order matching wow.export exactly
			FMatrix Result = FMatrix::Identity;

			// T(pivot)
			FMatrix PivotMat = FMatrix::Identity;
			PivotMat.M[3][0] = Px; PivotMat.M[3][1] = Py; PivotMat.M[3][2] = Pz;
			Result = Result * PivotMat;

			// T(translation)
			if (bHasTrans)
			{
				FVector T = SampleVec3(Idx, 0, EffAnimIdx, EffTimeMs, FVector::ZeroVector);
				FMatrix TransMat = FMatrix::Identity;
				TransMat.M[3][0] = T.X; TransMat.M[3][1] = T.Y; TransMat.M[3][2] = T.Z;
				Result = Result * TransMat;
			}

			// R(rotation)
			if (bHasRot)
			{
				FQuat Q = SampleQuat(Idx, EffAnimIdx, EffTimeMs);
				Result = Result * FQuatRotationMatrix(Q);
			}

			// S(scale)
			if (bHasScale || bHasScaleFallback)
			{
				int32 ScaleAnimIdx = bHasScale ? EffAnimIdx : 0;
				float ScaleTime = bHasScale ? EffTimeMs : 0.f;
				FVector S = SampleVec3(Idx, 1, ScaleAnimIdx, ScaleTime, FVector::OneVector);
				FMatrix ScaleMat = FMatrix::Identity;
				ScaleMat.M[0][0] = S.X; ScaleMat.M[1][1] = S.Y; ScaleMat.M[2][2] = S.Z;
				Result = Result * ScaleMat;
			}

			// T(-pivot)
			FMatrix NegPivotMat = FMatrix::Identity;
			NegPivotMat.M[3][0] = -Px; NegPivotMat.M[3][1] = -Py; NegPivotMat.M[3][2] = -Pz;
			Result = Result * NegPivotMat;

			LocalMat = Result;
		}

		// Parent chain: component = parent * local (column-major)
		if (ParentIdx >= 0 && ParentIdx < BoneCount)
			ComponentMatrices[Idx] = ComponentMatrices[ParentIdx] * LocalMat;
		else
			ComponentMatrices[Idx] = LocalMat;

		BoneCalculated[Idx] = true;
	};

	for (int32 i = 0; i < BoneCount; ++i)
		CalcBone(i);

	// Convert component-space WowLib matrices to UE bone-local FTransforms
	// WowLib→UE: position {z*100, x*100, y*100}, quat {qz, qx, qy, qw}, scale {sz, sx, sy}
	for (int32 i = 0; i < BoneCount; ++i)
	{
		// Extract local mat in WowLib space
		FMatrix WL_Local = FMatrix::Identity;
		int32 ParentIdx = Loader->bones[i].parentBone;
		if (ParentIdx >= 0 && ParentIdx < BoneCount)
			WL_Local = ComponentMatrices[ParentIdx].Inverse() * ComponentMatrices[i];
		else
			WL_Local = ComponentMatrices[i];

		// Extract TRS from WowLib local matrix
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

		// Convert to UE space
		FVector UE_Trans = WowLibToUE_Translation(WL_Trans.X, WL_Trans.Y, WL_Trans.Z);
		FQuat UE_Rot = WowLibToUE_Rotation(WL_Rot.X, WL_Rot.Y, WL_Rot.Z, WL_Rot.W);
		FVector UE_Scale = WowLibToUE_Scale(Sx, Sy, Sz);

		BoneLocalTransforms[i] = FTransform(UE_Rot, UE_Trans, UE_Scale);
	}
}
