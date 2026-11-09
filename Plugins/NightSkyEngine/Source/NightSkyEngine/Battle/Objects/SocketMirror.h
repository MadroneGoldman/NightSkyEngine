// Madrone change: exact socket mirroring for side-swapped fighters.

#pragma once

#include "CoreMinimal.h"
#include "SocketMirror.generated.h"

class USkeletalMeshComponent;
class UMirrorDataTable;

/**
 * Which local axis of an object its shape is symmetric across (the plane perpendicular to that axis splits the
 * object into two mirror halves). Used when mirroring a socket-following object without negative scale: a rotation
 * can't turn a right glove into a left glove, but it CAN produce the exact mirror image of anything that is symmetric
 * across one of its own planes. A cigarette modelled along X is symmetric across its Y and Z planes, so Y (the
 * default) or Z both give an exact mirror image; X would point the lit end the wrong way.
 */
UENUM(BlueprintType)
enum class ESocketMirrorSymmetryAxis : uint8
{
	X,
	Y,
	Z,
};

/**
 * Mirrors socket transforms on fighters whose LEFT side is drawn with MIRRORED ANIMATIONS.
 *
 * WHY THIS EXISTS
 * ---------------
 * Night Sky fighters swap sides "physically accurately": facing left, the anim graph runs the pose through a mirror
 * node (ABP_Common / ABP_BattleCommon, bShouldMirror = Direction == DIR_Left, mirror table MDT_SK_Manny /
 * MDT_NarrativeManny, axis X). There is no negative actor scale, no inverted physics asset and no material trick,
 * because ragdolls (throws) need a normal, unscaled skeletal mesh.
 *
 * A mirror node swaps the roles of the left and right bones. The animation that holds a cigarette in the right
 * hand (index_03_r) holds it in the LEFT hand (index_03_l) once mirrored. So an object that simply follows the
 * "index_03_r" socket ends up in the wrong hand, and even following "index_03_l" isn't enough: the socket's own
 * offset/rotation and the object's offsets were authored for the right hand, whose bone axes point differently.
 *
 * THE METHOD
 * ----------
 * Facing left, the object must sit exactly where the MIRROR IMAGE of its right-side placement would be. We compute
 * that directly from the mirrored pose:
 *
 *   1. Pair bone.   The socket's bone B (e.g. index_03_r) has a mirror partner P (index_03_l), found with the mirror
 *                   table or the same _l/_r naming rules the table uses. Centre bones are their own partner.
 *   2. Unmirror.    In the mirrored pose, P was produced from B's unmirrored animation. UE's mirror node
 *                   (FAnimationRuntime::MirrorPose) does, in component space:
 *                       Pos(P) = Reflect(Pos(B))
 *                       Rot(P) = MirrorQuat(Rot(B)) * MirrorQuat(RefRot(B))^-1 * RefRot(P)
 *                   (RefRot = component-space rotation of the bone in the mesh's reference pose; the trailing term
 *                   corrects for left and right bones having differently oriented axes.) Mirroring is its own
 *                   inverse, so we solve for B's UNMIRRORED component-space transform:
 *                       Pos(B) = Reflect(Pos(P))
 *                       Rot(B) = MirrorQuat( Rot(P) * RefRot(P)^-1 * MirrorQuat(RefRot(B)) )
 *   3. Place.       Apply the socket (authored on B) and the object's SocketOffset / SocketRotationOffset exactly as
 *                   the right side does. This is where the object WOULD be if the frame weren't mirrored.
 *   4. Mirror back. Reflect that transform across the same axis: position reflected, rotation conjugated
 *                   (MirrorQuat). Conjugating also reflects the object's own X axis, so a final 180 degree turn maps
 *                   that onto the object's chosen symmetry plane (see ESocketMirrorSymmetryAxis). The result is a
 *                   proper rotation (no negative scale) that shows the exact mirror image of a symmetric object.
 *
 * Everything happens in the skeletal mesh component's space, the space the mirror node works in, so it doesn't
 * matter how the mesh is rotated in its actor or where the battle sits in the world.
 *
 * Overview, test tools and results: WitchTools Docs/SideSwap.md.
 *
 * Visual only: nothing here is rollback state. Pair bones and reference rotations are cached per mesh asset.
 */
struct NIGHTSKYENGINE_API FSocketMirror
{
	/**
	 * Component-space transform of SocketName on Mesh plus an object offset, mirrored if bMirrored.
	 *
	 * @param Mesh            The fighter mesh the object follows (its pose must be the mirrored one when bMirrored).
	 * @param SocketName      A socket or bone name, as authored for the RIGHT side (e.g. "index_03_r").
	 * @param ObjectOffset    The object's SocketOffset / SocketRotationOffset, relative to the socket.
	 * @param bMirrored       True when the mesh is facing left with mirrored animations.
	 * @param MirrorTable     The table the anim graph mirrors with (optional: naming rules + MirrorAxis are used
	 *                        without one). Must match the anim graph's for an exact result.
	 * @param MirrorAxis      Component-space axis the animations are mirrored across (the table's, when given).
	 * @param SymmetryAxis    The object's own symmetry axis (step 4 above).
	 * @param OutTransform    Result, in Mesh's component space.
	 * @return false if the socket/bone doesn't exist on Mesh.
	 */
	static bool GetComponentSpaceTransform(const USkeletalMeshComponent* Mesh, FName SocketName,
	                                       const FTransform& ObjectOffset, bool bMirrored,
	                                       const UMirrorDataTable* MirrorTable, EAxis::Type MirrorAxis,
	                                       ESocketMirrorSymmetryAxis SymmetryAxis, FTransform& OutTransform);

	/** Same, in world space (component space * the mesh's component-to-world). */
	static bool GetWorldTransform(const USkeletalMeshComponent* Mesh, FName SocketName,
	                              const FTransform& ObjectOffset, bool bMirrored,
	                              const UMirrorDataTable* MirrorTable, EAxis::Type MirrorAxis,
	                              ESocketMirrorSymmetryAxis SymmetryAxis, FTransform& OutTransform);

	/** The mirror partner of BoneName with the default _l/_r naming rules (the rules MDT_SK_Manny uses).
	 * Returns BoneName itself for centre bones. Exposed for tests. */
	static FName MirrorBoneNameByRules(FName BoneName);
};
