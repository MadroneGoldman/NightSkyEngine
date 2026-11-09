// Madrone change: exact socket mirroring for side-swapped fighters. See SocketMirror.h for the method.

#include "SocketMirror.h"

#include "AnimationRuntime.h"
#include "Animation/MirrorDataTable.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkinnedAsset.h"
#include "ReferenceSkeleton.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SocketMirror)

namespace SocketMirrorPrivate
{
	/**
	 * Per mesh asset (and mirror table) data that never changes during a fight:
	 *  - MirrorBone[i]: mesh bone index of bone i's mirror partner (i itself for centre bones).
	 *  - RefRotationCS[i]: component-space rotation of bone i in the mesh's reference pose. The mirror node uses the
	 *    same values to correct for left/right bones having differently oriented axes.
	 */
	struct FMeshMirrorData
	{
		TArray<int32> MirrorBone;
		TArray<FQuat> RefRotationCS;
	};

	using FCacheKey = TPair<TObjectKey<const USkinnedAsset>, TObjectKey<const UMirrorDataTable>>;

	// Game-thread only (battle visuals run on the game thread). Entries are tiny; assets rarely change mid-session,
	// and a reimported mesh gets a fresh asset key anyway.
	TMap<FCacheKey, FMeshMirrorData>& Cache()
	{
		static TMap<FCacheKey, FMeshMirrorData> Map;
		return Map;
	}

	const FMeshMirrorData& GetMirrorData(const USkinnedAsset* Asset, const UMirrorDataTable* Table)
	{
		const FCacheKey Key(Asset, Table);
		if (const FMeshMirrorData* Found = Cache().Find(Key))
		{
			return *Found;
		}

		FMeshMirrorData Data;
		const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
		const int32 NumBones = RefSkeleton.GetNum();
		const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();

		// Reference pose rotations in component space: parent * local, walking down the hierarchy (parents always
		// come before their children in a reference skeleton). Same as FAnimationRuntime::MirrorPose builds them.
		Data.RefRotationCS.SetNum(NumBones);
		for (int32 Bone = 0; Bone < NumBones; ++Bone)
		{
			const int32 Parent = RefSkeleton.GetParentIndex(Bone);
			Data.RefRotationCS[Bone] = Parent == INDEX_NONE
				? RefPose[Bone].GetRotation()
				: Data.RefRotationCS[Parent] * RefPose[Bone].GetRotation();
		}

		// Mirror partners. Start with every bone as its own partner (centre bones), then pair by the table's rows
		// when there is a table (exactly what the anim graph uses), else by the table's naming rules.
		Data.MirrorBone.SetNum(NumBones);
		for (int32 Bone = 0; Bone < NumBones; ++Bone)
		{
			Data.MirrorBone[Bone] = Bone;
		}
		bool bPairedByTable = false;
		if (Table)
		{
			Table->ForeachRow<FMirrorTableRow>(TEXT("SocketMirror"), [&](const FName&, const FMirrorTableRow& Row)
			{
				if (!Row.bEnabled || Row.MirrorEntryType != EMirrorRowType::Bone)
				{
					return;
				}
				const int32 From = RefSkeleton.FindBoneIndex(Row.Name);
				const int32 To = RefSkeleton.FindBoneIndex(Row.MirroredName);
				if (From != INDEX_NONE && To != INDEX_NONE)
				{
					Data.MirrorBone[From] = To;
					bPairedByTable = true;
				}
			});
		}
		if (!bPairedByTable)
		{
			for (int32 Bone = 0; Bone < NumBones; ++Bone)
			{
				const int32 Partner = RefSkeleton.FindBoneIndex(FSocketMirror::MirrorBoneNameByRules(RefSkeleton.GetBoneName(Bone)));
				if (Partner != INDEX_NONE)
				{
					Data.MirrorBone[Bone] = Partner;
				}
			}
		}

		return Cache().Add(Key, MoveTemp(Data));
	}

	/**
	 * Rotation that turns "reflected across the component mirror axis" into "reflected across the object's own
	 * symmetry plane" (step 4 in SocketMirror.h).
	 *
	 * Conjugating a rotation R by the reflection S (MirrorQuat) gives S * R * S. Read from the right, the trailing S
	 * reflects the object's LOCAL axis with the same index as MirrorAxis. We want the object's symmetry axis
	 * reflected instead: S * R * S_sym = (S * R * S) * (S * S_sym), and two reflections across perpendicular planes
	 * are a 180 degree turn about the axis perpendicular to both. Same axis: no correction.
	 */
	FQuat SymmetryCorrection(EAxis::Type MirrorAxis, ESocketMirrorSymmetryAxis SymmetryAxis)
	{
		const int32 Mirror = MirrorAxis == EAxis::X ? 0 : MirrorAxis == EAxis::Y ? 1 : 2;
		const int32 Symmetry = static_cast<int32>(SymmetryAxis);
		if (Mirror == Symmetry)
		{
			return FQuat::Identity;
		}
		const int32 Third = 3 - Mirror - Symmetry;  // the axis perpendicular to both
		const FVector Axis = Third == 0 ? FVector::XAxisVector : Third == 1 ? FVector::YAxisVector : FVector::ZAxisVector;
		return FQuat(Axis, UE_PI);
	}
}

FName FSocketMirror::MirrorBoneNameByRules(FName BoneName)
{
	// The find/replace rules of MDT_SK_Manny / MDT_NarrativeManny, applied to whole prefixes/suffixes only (never
	// to letters inside the name: "upperarm_r" must become "upperarm_l", not "uppelalm_l").
	const FString Name = BoneName.ToString();
	static const TPair<const TCHAR*, const TCHAR*> Suffixes[] = {{TEXT("_l"), TEXT("_r")}, {TEXT("_r"), TEXT("_l")},
	                                                             {TEXT("_L"), TEXT("_R")}, {TEXT("_R"), TEXT("_L")}};
	for (const auto& Rule : Suffixes)
	{
		if (Name.EndsWith(Rule.Key, ESearchCase::CaseSensitive))
		{
			return FName(*(Name.LeftChop(FCString::Strlen(Rule.Key)) + Rule.Value));
		}
	}
	static const TPair<const TCHAR*, const TCHAR*> Prefixes[] = {{TEXT("l_"), TEXT("r_")}, {TEXT("r_"), TEXT("l_")},
	                                                             {TEXT("L_"), TEXT("R_")}, {TEXT("R_"), TEXT("L_")}};
	for (const auto& Rule : Prefixes)
	{
		if (Name.StartsWith(Rule.Key, ESearchCase::CaseSensitive))
		{
			return FName(*(FString(Rule.Value) + Name.RightChop(FCString::Strlen(Rule.Key))));
		}
	}
	return BoneName;
}

bool FSocketMirror::GetComponentSpaceTransform(const USkeletalMeshComponent* Mesh, FName SocketName,
                                               const FTransform& ObjectOffset, bool bMirrored,
                                               const UMirrorDataTable* MirrorTable, EAxis::Type MirrorAxis,
                                               ESocketMirrorSymmetryAxis SymmetryAxis, FTransform& OutTransform)
{
	using namespace SocketMirrorPrivate;

	const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	if (!Asset || !Mesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	// Facing right (or no mirrored animation): the plain socket, then the object's offset relative to it.
	// FTransform composition reads left to right as child-to-parent: Offset, then socket, then component.
	if (!bMirrored)
	{
		OutTransform = ObjectOffset * Mesh->GetSocketTransform(SocketName, RTS_Component);
		return true;
	}

	if (MirrorTable)
	{
		MirrorAxis = MirrorTable->MirrorAxis;
	}
	if (MirrorAxis == EAxis::None)
	{
		MirrorAxis = EAxis::X;
	}

	// The socket's bone B and the socket's own transform relative to it (identity when SocketName is a bone).
	FName BoneName = SocketName;
	FTransform SocketLocal = FTransform::Identity;
	if (const USkeletalMeshSocket* Socket = Mesh->GetSocketByName(SocketName))
	{
		BoneName = Socket->BoneName;
		SocketLocal = Socket->GetSocketLocalTransform();
	}

	const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
	const int32 BoneB = RefSkeleton.FindBoneIndex(BoneName);
	if (BoneB == INDEX_NONE)
	{
		return false;
	}
	const FMeshMirrorData& Data = GetMirrorData(Asset, MirrorTable);
	const int32 BoneP = Data.MirrorBone[BoneB];

	// Step 1: P's transform in the current (mirrored) pose, in component space. GetSocketTransform also works for
	// bones and follows a leader pose component, so this is right for follower meshes too.
	const FTransform PoseP = Mesh->GetSocketTransform(RefSkeleton.GetBoneName(BoneP), RTS_Component);

	// Step 2: B's UNMIRRORED transform. Invert the mirror node's component-space mapping (SocketMirror.h):
	//   Pos(B) = Reflect(Pos(P))
	//   Rot(B) = MirrorQuat( Rot(P) * RefRot(P)^-1 * MirrorQuat(RefRot(B)) )
	// Scale is untouched by mirroring.
	const FQuat RefB = Data.RefRotationCS[BoneB];
	const FQuat RefP = Data.RefRotationCS[BoneP];
	FTransform UnmirroredB;
	UnmirroredB.SetLocation(FAnimationRuntime::MirrorVector(PoseP.GetLocation(), MirrorAxis));
	UnmirroredB.SetRotation(FAnimationRuntime::MirrorQuat(PoseP.GetRotation() * RefP.Inverse() * FAnimationRuntime::MirrorQuat(RefB, MirrorAxis), MirrorAxis).GetNormalized());
	UnmirroredB.SetScale3D(PoseP.GetScale3D());

	// Step 3: where the object would be on the right side: offset -> socket -> bone B, the same chain as above.
	const FTransform RightSide = ObjectOffset * SocketLocal * UnmirroredB;

	// Step 4: reflect it back across the mirror axis. Position: reflected. Rotation: conjugated (MirrorQuat), then
	// the 180 degree correction so the reflected plane is the object's symmetry plane. Result: a proper rotation
	// showing the exact mirror image of a symmetric object, with positive scale.
	OutTransform.SetLocation(FAnimationRuntime::MirrorVector(RightSide.GetLocation(), MirrorAxis));
	OutTransform.SetRotation((FAnimationRuntime::MirrorQuat(RightSide.GetRotation(), MirrorAxis) * SymmetryCorrection(MirrorAxis, SymmetryAxis)).GetNormalized());
	OutTransform.SetScale3D(RightSide.GetScale3D().GetAbs());
	return true;
}

bool FSocketMirror::GetWorldTransform(const USkeletalMeshComponent* Mesh, FName SocketName,
                                      const FTransform& ObjectOffset, bool bMirrored,
                                      const UMirrorDataTable* MirrorTable, EAxis::Type MirrorAxis,
                                      ESocketMirrorSymmetryAxis SymmetryAxis, FTransform& OutTransform)
{
	FTransform ComponentSpace;
	if (!GetComponentSpaceTransform(Mesh, SocketName, ObjectOffset, bMirrored, MirrorTable, MirrorAxis, SymmetryAxis, ComponentSpace))
	{
		return false;
	}
	OutTransform = ComponentSpace * Mesh->GetComponentTransform();
	return true;
}
