#pragma once

#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"

#include "AnimNode_TwistBones.generated.h"

#define UE_API ANIMNODES_API

/**
 * One helper bone the node poses, and where its pose comes from.
 *
 * Rotation: the bone's local rotation is replaced by a fraction of how far RotationSource has turned from its reference
 * pose, split into twist about the node's TwistAxis and the swing left over. TwistWeight is the fraction of the twist
 * the bone takes; negative values turn it the other way, which is how a twist bone near the shoulder undoes the
 * upper arm's roll. CounterSwingWeight is the fraction of the swing it undoes, for corrective roots that should lag
 * behind the joint they sit on. Without a RotationSource the bone keeps the rotation it arrived with.
 *
 * Placement: with SpanStart and SpanEnd set, the bone is moved to that fraction of the way between them, so twist bones
 * stay spread along a limb whatever the animation does to its length. Without them the bone keeps its translation.
 */
USTRUCT(BlueprintType)
struct FTwistBoneSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Twist")
	FBoneReference Bone;

	UPROPERTY(EditAnywhere, Category = "Twist|Rotation")
	FBoneReference RotationSource;

	UPROPERTY(EditAnywhere, Category = "Twist|Rotation", meta = (UIMin = -1.0, UIMax = 1.0))
	float TwistWeight = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Twist|Rotation", meta = (UIMin = 0.0, UIMax = 1.0))
	float CounterSwingWeight = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Twist|Placement")
	FBoneReference SpanStart;

	UPROPERTY(EditAnywhere, Category = "Twist|Placement")
	FBoneReference SpanEnd;

	UPROPERTY(EditAnywhere, Category = "Twist|Placement", meta = (UIMin = 0.0, UIMax = 1.0))
	float SpanAlpha = 0.5f;
};

/**
 * FAnimNode_TwistBones
 *
 * Poses the helper bones a skinned limb carries — twist bones and corrective roots — from the joints the animation
 * moves, so the skin spreads a turn along the limb instead of wringing it at the joint. Every bone is a row of data
 * (FTwistBoneSetup), so the same node serves any skeleton that has such bones.
 *
 * It reads only the bones the animation drives and writes only the helper bones, so the order of the rows does not
 * change the result; they are evaluated parent first all the same, in case a setup drives a bone under another.
 */
USTRUCT(BlueprintInternalUseOnly)
struct FAnimNode_TwistBones : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** The axis, in each source bone's local space, that counts as twist. Down the bone on UE mannequins. */
	UPROPERTY(EditAnywhere, Category = "Twist")
	FVector TwistAxis = FVector::XAxisVector;

	UPROPERTY(EditAnywhere, Category = "Twist")
	TArray<FTwistBoneSetup> Bones;

	//~FAnimNode_SkeletalControlBase interface
	UE_API virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	UE_API virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	UE_API virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	UE_API virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	//~End of FAnimNode_SkeletalControlBase interface

	/** Indices into Bones, parents first: the order FBoneTransform output has to be in. Rebuilt with the bone references. */
	TArray<int32> EvaluationOrder;
};

#undef UE_API
