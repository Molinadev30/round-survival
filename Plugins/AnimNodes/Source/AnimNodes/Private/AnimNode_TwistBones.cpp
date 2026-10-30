#include "AnimNode_TwistBones.h"

#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_TwistBones)

namespace TwistBones
{
	// A fraction of a rotation, measured from no rotation at all; a negative fraction turns the other way.
	FQuat ScaleRotation(const FQuat& Rotation, float Weight)
	{
		const FQuat Directed = Weight < 0.0f ? Rotation.Inverse() : Rotation;
		return FQuat::Slerp(FQuat::Identity, Directed, FMath::Abs(Weight));
	}
}

void FAnimNode_TwistBones::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(%d bones)"), EvaluationOrder.Num());
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_TwistBones::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FVector Axis = TwistAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::XAxisVector);

	for (const int32 SetupIndex : EvaluationOrder)
	{
		const FTwistBoneSetup& Setup = Bones[SetupIndex];
		const FCompactPoseBoneIndex BoneIndex = Setup.Bone.GetCompactPoseIndex(BoneContainer);
		const FCompactPoseBoneIndex ParentIndex = BoneContainer.GetParentBoneIndex(BoneIndex);

		// A parent posed earlier in this pass has not been written to the pose yet.
		FTransform ParentTransform = Output.Pose.GetComponentSpaceTransform(ParentIndex);
		if (const FBoneTransform* PosedParent = OutBoneTransforms.FindByPredicate([ParentIndex](const FBoneTransform& Posed) { return Posed.BoneIndex == ParentIndex; }))
		{
			ParentTransform = PosedParent->Transform;
		}

		FTransform LocalTransform = Output.Pose.GetLocalSpaceTransform(BoneIndex);

		if (Setup.RotationSource.IsValidToEvaluate(BoneContainer))
		{
			const FCompactPoseBoneIndex SourceIndex = Setup.RotationSource.GetCompactPoseIndex(BoneContainer);
			const FQuat SourceRotation = Output.Pose.GetLocalSpaceTransform(SourceIndex).GetRotation();
			const FQuat SourceReference = BoneContainer.GetRefPoseTransform(SourceIndex).GetRotation();

			FQuat Swing;
			FQuat Twist;
			(SourceReference.Inverse() * SourceRotation).ToSwingTwist(Axis, Swing, Twist);

			LocalTransform.SetRotation(TwistBones::ScaleRotation(Twist, Setup.TwistWeight)
				* TwistBones::ScaleRotation(Swing.Inverse(), Setup.CounterSwingWeight));
		}

		FTransform ComponentTransform = LocalTransform * ParentTransform;

		if (Setup.SpanStart.IsValidToEvaluate(BoneContainer) && Setup.SpanEnd.IsValidToEvaluate(BoneContainer))
		{
			const FVector Start = Output.Pose.GetComponentSpaceTransform(Setup.SpanStart.GetCompactPoseIndex(BoneContainer)).GetLocation();
			const FVector End = Output.Pose.GetComponentSpaceTransform(Setup.SpanEnd.GetCompactPoseIndex(BoneContainer)).GetLocation();
			ComponentTransform.SetLocation(FMath::Lerp(Start, End, Setup.SpanAlpha));
		}

		OutBoneTransforms.Add(FBoneTransform(BoneIndex, ComponentTransform));
	}
}

bool FAnimNode_TwistBones::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return EvaluationOrder.Num() > 0;
}

void FAnimNode_TwistBones::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	EvaluationOrder.Reset();

	for (int32 SetupIndex = 0; SetupIndex < Bones.Num(); ++SetupIndex)
	{
		FTwistBoneSetup& Setup = Bones[SetupIndex];
		Setup.Bone.Initialize(RequiredBones);
		Setup.RotationSource.Initialize(RequiredBones);
		Setup.SpanStart.Initialize(RequiredBones);
		Setup.SpanEnd.Initialize(RequiredBones);

		// The root has no parent to pose it against, and a bone cut by the LOD is not in the pose at all.
		if (Setup.Bone.IsValidToEvaluate(RequiredBones) && Setup.Bone.GetCompactPoseIndex(RequiredBones) != FCompactPoseBoneIndex(0))
		{
			EvaluationOrder.Add(SetupIndex);
		}
	}

	EvaluationOrder.Sort([this, &RequiredBones](int32 A, int32 B)
	{
		return Bones[A].Bone.GetCompactPoseIndex(RequiredBones) < Bones[B].Bone.GetCompactPoseIndex(RequiredBones);
	});
}
