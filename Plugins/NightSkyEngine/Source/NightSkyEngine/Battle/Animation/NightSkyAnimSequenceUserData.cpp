// Fill out your copyright notice in the Description page of Project Settings.


#include "NightSkyAnimSequenceUserData.h"

#include "NightSkyAnimMetaData.h"
#include "NightSkyEngine/Battle/Misc/Globals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyAnimSequenceUserData)

void UNightSkyAnimSequenceUserData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << MeshSpaceBoneTransforms;
	Ar << RootTranslation;
}

void UNightSkyAnimSequenceUserData::PostEditChangeOwner(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeOwner(PropertyChangedEvent);

	// We can't call blueprint implemented functions while routing post load
	if (FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		return;
	}

	// Skip interactive changes
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	MeshSpaceBoneTransforms.Empty();
	RootTranslation.Empty();

	auto AnimSequence = Cast<UAnimSequence>(GetOuter());
	if (!IsValid(AnimSequence)) return;

	UNightSkyAnimMetaData* AnimMetaData = nullptr;

	for (auto& MetaData : AnimSequence->GetMetaData())
	{
		AnimMetaData = Cast<UNightSkyAnimMetaData>(MetaData);
		if (IsValid(MetaData)) break;
	}

	if (!IsValid(AnimMetaData))
	{
		ConditionalBeginDestroy();
		return;
	}

	FrameCount = AnimSequence->GetPlayLength() * AnimSequence->GetSamplingFrameRate().AsDecimal();
	FrameRate = AnimSequence->GetSamplingFrameRate().AsDecimal();

	auto ReferenceSkeleton = AnimSequence->GetSkeleton()->GetReferenceSkeleton();

	for (auto& BoneName : AnimMetaData->CachedBoneNames)
	{
		if (ReferenceSkeleton.FindBoneIndex(BoneName) == INDEX_NONE) continue;
		TMap<int32, FAnimTransform> BoneTransforms;

		for (int i = 0; i < FrameCount; i++)
		{
			FTransform Transform;
			FAnimExtractContext Context{
				static_cast<double>(i) / AnimSequence->GetSamplingFrameRate().AsDecimal()
			};
			AnimSequence->GetBoneTransform(
				Transform, FSkeletonPoseBoneIndex(ReferenceSkeleton.FindBoneIndex(BoneName)),
				Context, true);

			FAnimTransform AnimTransform;
			AnimTransform.Position.X = Transform.GetTranslation().X * COORD_SCALE;
			AnimTransform.Position.Y = Transform.GetTranslation().Y * COORD_SCALE;
			AnimTransform.Position.Z = Transform.GetTranslation().Z * COORD_SCALE;

			auto Rotation = Transform.GetRotation().Rotator();
			AnimTransform.Rotation.Pitch = Rotation.Pitch * 1000;
			AnimTransform.Rotation.Yaw = Rotation.Yaw * 1000;
			AnimTransform.Rotation.Roll = Rotation.Roll * 1000;

			AnimTransform.Scale.X = Transform.GetScale3D().X * 1000;
			AnimTransform.Scale.Y = Transform.GetScale3D().Y * 1000;
			AnimTransform.Scale.Z = Transform.GetScale3D().Z * 1000;

			BoneTransforms.Add(i, AnimTransform);
		}

		MeshSpaceBoneTransforms.Add(BoneName, BoneTransforms);
	}

	for (int i = 0; i < FrameCount; i++)
	{
		FAnimExtractContext Context{
			static_cast<double>(i) / AnimSequence->GetSamplingFrameRate().AsDecimal(),
			true,
			FDeltaTimeRecord{1.0f / (float)AnimSequence->GetSamplingFrameRate().AsDecimal()},
		};
		FTransform transform = AnimSequence->ExtractRootMotion(Context);
		FVector Translation = transform.GetTranslation();
		FAnimVector AnimTranslation;

		AnimTranslation.X = Translation.X * COORD_SCALE;
		AnimTranslation.Y = Translation.Y * COORD_SCALE;
		AnimTranslation.Z = Translation.Z * COORD_SCALE;

		RootTranslation.Add(i, AnimTranslation);
	}
}

FAnimVector UNightSkyAnimSequenceUserData::GetRootTranslationAtFrame(int32 Frame) const
{
	if (!RootTranslation.Contains(Frame)) return FAnimVector{};
	return RootTranslation[Frame];
}

FAnimVector UNightSkyAnimSequenceUserData::GetRootTranslationAtFrame60(int32 Frame) const
{
	return GetRootTranslationAtTime(Frame * FrameRate);
}

FAnimVector UNightSkyAnimSequenceUserData::GetRootTranslationAtTime(int64 Time) const
{
	if (Time / 60 >= FrameCount - 1) return RootTranslation[RootTranslation.Num() - 1];

	const auto RoundedToFrame = Time / 60;

	const auto PrevFrame = Time / 60;
	const auto NextFrame = Time / 60 + 1;

	// Time is in 1/60ths of an animation frame, so (Time - RoundedToFrame * 60) is the progress through the
	// current frame, in [0, 60). LerpAnimVector expects an alpha in [0, 1000]. The old expression
	// "(Time - RoundedToFrame * 60) / (60 * FrameCount) * 1000" divided by FrameCount as well, and divided
	// before multiplying. Integer division made it 0 almost always, so root motion never interpolated
	// between frames. Multiply first, then divide by 60 only.
	return LerpAnimVector(RootTranslation[PrevFrame], RootTranslation[NextFrame],
	                      (Time - RoundedToFrame * 60) * 1000 / 60);
}

bool UNightSkyAnimSequenceUserData::HasRootMotion() const
{
	for (const auto& [Frame, Translation] : RootTranslation)
	{
		if (Translation.X != 0 || Translation.Y != 0 || Translation.Z != 0) return true;
	}
	return false;
}

FAnimTransform UNightSkyAnimSequenceUserData::GetCachedBoneTransformAtTime(FName BoneName, int32 Time,
                                                                           bool bRelativeToRoot) const
{
	if (!MeshSpaceBoneTransforms.Contains(BoneName)) return FAnimTransform{};
	if (!MeshSpaceBoneTransforms[BoneName].Contains(Time)) return FAnimTransform{};
	auto Transform = MeshSpaceBoneTransforms[BoneName][Time];

	if (!bRelativeToRoot) return Transform;
	if (!RootTranslation.Contains(Time)) return Transform;

	auto Root = RootTranslation[Time];
	Transform.Position.X += Root.X;
	Transform.Position.Y += Root.Y;
	Transform.Position.Z += Root.Z;
	return Transform;
}

FAnimVector UNightSkyAnimSequenceUserData::GetCachedBoneLocationAtTime(FName BoneName, int32 Time,
                                                                       bool bRelativeToRoot) const
{
	if (!MeshSpaceBoneTransforms.Contains(BoneName)) return FAnimVector{};
	if (!MeshSpaceBoneTransforms[BoneName].Contains(Time)) return FAnimVector{};
	auto Transform = MeshSpaceBoneTransforms[BoneName][Time];

	if (!bRelativeToRoot) return Transform.Position;
	if (!RootTranslation.Contains(Time)) return Transform.Position;

	auto Root = RootTranslation[Time];
	Transform.Position.X += Root.X;
	Transform.Position.Y += Root.Y;
	Transform.Position.Z += Root.Z;
	return Transform.Position;
}
