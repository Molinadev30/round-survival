#pragma once

#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"

#include "AnimNode_ProceduralAimOffset.generated.h"

#define UE_API ANIMNODES_API

/** One bone of the bend, and how much of the pitch it turns by itself. */
USTRUCT(BlueprintType)
struct FAimOffsetBendBone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Aim")
	FBoneReference Bone;

	/** The fraction of the pitch this bone adds, on top of what its parents have turned. */
	UPROPERTY(EditAnywhere, Category = "Aim", meta = (UIMin = 0.0, UIMax = 1.0))
	float Weight = 0.0f;
};

/** How the aim step turns its pivot. */
UENUM(BlueprintType)
enum class EAimOffsetAimMode : uint8
{
	/**
	 * Until AimReferenceAxis on AimReference points at exactly Pitch, whatever the animation underneath does: a head
	 * that looks where the view does.
	 */
	Absolute,

	/**
	 * By the share of Pitch the bend did not already give the pivot through its parents, so the pivot ends up turned
	 * by the whole of Pitch on top of the animation: a weapon that aims with the view and still moves with a reload.
	 */
	Remainder,
};

/**
 * FAnimNode_ProceduralAimOffset
 *
 * Pitches a pose to a view angle without aim-offset animations, in two steps:
 *
 * - **Bend.** Each row turns its bone, about its own origin, by its weight's share of Pitch. The turns add up down the
 *   chain, so a spine with a small share per vertebra bends rather than hinging at one joint.
 * - **Aim.** AimPivot is then turned about its own origin, as AimMode says: a weapon held from a pivot at the
 *   shoulders, or a head looking for itself.
 *
 * Pitch is measured in component space, from ForwardAxis towards the up axis, about PitchAxis; positive is up.
 * Either step can be left empty. The node's Alpha blends the whole result.
 */
USTRUCT(BlueprintInternalUseOnly)
struct FAnimNode_ProceduralAimOffset : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** The angle to aim at, in degrees; positive is up. */
	UPROPERTY(EditAnywhere, Category = "Aim", meta = (PinShownByDefault))
	float Pitch = 0.0f;

	/** Component-space axis the pose pitches about: the mesh's right. X on UE mannequins. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	FVector PitchAxis = FVector::XAxisVector;

	/** Component-space direction that counts as level: the mesh's forward. Y on UE mannequins. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	FVector ForwardAxis = FVector::YAxisVector;

	UPROPERTY(EditAnywhere, Category = "Aim|Bend")
	TArray<FAimOffsetBendBone> BendBones;

	/** The bone the aim step turns. None skips the step. */
	UPROPERTY(EditAnywhere, Category = "Aim|Target")
	FBoneReference AimPivot;

	UPROPERTY(EditAnywhere, Category = "Aim|Target")
	EAimOffsetAimMode AimMode = EAimOffsetAimMode::Absolute;

	/** Absolute only: the bone whose axis has to point at the angle, the pivot itself or a bone under it. */
	UPROPERTY(EditAnywhere, Category = "Aim|Target", meta = (EditCondition = "AimMode == EAimOffsetAimMode::Absolute"))
	FBoneReference AimReference;

	/** Absolute only: the direction on AimReference, in that bone's own space, that should point at the angle. */
	UPROPERTY(EditAnywhere, Category = "Aim|Target", meta = (EditCondition = "AimMode == EAimOffsetAimMode::Absolute"))
	FVector AimReferenceAxis = FVector::YAxisVector;

	//~FAnimNode_SkeletalControlBase interface
	UE_API virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	UE_API virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	UE_API virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	UE_API virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	//~End of FAnimNode_SkeletalControlBase interface

	/** Indices into BendBones that are in the pose, parents first. Rebuilt with the bone references. */
	TArray<int32> BendOrder;

	/** Whether the aim step has the bones its mode needs in the pose. */
	bool bCanAim = false;

	/** Remainder only: the weights of the bend rows on the pivot or above it, added up. Rebuilt with the bone references. */
	float PivotBendShare = 0.0f;
};

#undef UE_API
