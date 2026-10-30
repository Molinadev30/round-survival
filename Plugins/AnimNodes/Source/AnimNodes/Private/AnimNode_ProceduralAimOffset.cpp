#include "AnimNode_ProceduralAimOffset.h"

#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ProceduralAimOffset)

void FAnimNode_ProceduralAimOffset::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Pitch: %.1f, %d bend bones%s)"), Pitch, BendOrder.Num(), bCanAim ? TEXT(", aiming") : TEXT(""));
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_ProceduralAimOffset::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FVector Axis = PitchAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::XAxisVector);

	// Bones turned so far, by compact index. A bone below one of them has not moved in the pose yet: its component
	// transform is rebuilt from its local one down from the nearest turned ancestor.
	TMap<int32, FTransform> Posed;
	const auto ComponentTransform = [&Output, &BoneContainer, &Posed](FCompactPoseBoneIndex Index)
	{
		TArray<FCompactPoseBoneIndex, TInlineAllocator<16>> Chain;
		FCompactPoseBoneIndex Current = Index;
		while (Current != INDEX_NONE && !Posed.Contains(Current.GetInt()))
		{
			Chain.Add(Current);
			Current = BoneContainer.GetParentBoneIndex(Current);
		}

		if (Current == INDEX_NONE)
		{
			return Output.Pose.GetComponentSpaceTransform(Index);
		}

		FTransform Transform = Posed[Current.GetInt()];
		for (int32 Link = Chain.Num() - 1; Link >= 0; --Link)
		{
			Transform = Output.Pose.GetLocalSpaceTransform(Chain[Link]) * Transform;
		}
		return Transform;
	};

	for (const int32 Row : BendOrder)
	{
		const FAimOffsetBendBone& Bend = BendBones[Row];
		const FCompactPoseBoneIndex BoneIndex = Bend.Bone.GetCompactPoseIndex(BoneContainer);

		FTransform Transform = ComponentTransform(BoneIndex);
		Transform.SetRotation(FQuat(Axis, FMath::DegreesToRadians(Pitch * Bend.Weight)) * Transform.GetRotation());
		Posed.Add(BoneIndex.GetInt(), Transform);
	}

	if (bCanAim)
	{
		const FCompactPoseBoneIndex PivotIndex = AimPivot.GetCompactPoseIndex(BoneContainer);
		FTransform Pivot = ComponentTransform(PivotIndex);

		float Turn = Pitch * (1.0f - PivotBendShare);
		if (AimMode == EAimOffsetAimMode::Absolute)
		{
			// Measured in the plane the pitch turns in, so a reference angled sideways still reads its true elevation.
			const FTransform Reference = ComponentTransform(AimReference.GetCompactPoseIndex(BoneContainer));
			const FVector Forward = ForwardAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::YAxisVector);
			const FVector Up = FVector::CrossProduct(Axis, Forward).GetSafeNormal();
			const FVector Direction = Reference.TransformVectorNoScale(AimReferenceAxis);
			Turn = Pitch - FMath::RadiansToDegrees(FMath::Atan2(Direction | Up, Direction | Forward));
		}

		Pivot.SetRotation(FQuat(Axis, FMath::DegreesToRadians(Turn)) * Pivot.GetRotation());
		Posed.Add(PivotIndex.GetInt(), Pivot);
	}

	// FBoneTransform output has to be sorted by bone index.
	TArray<int32> Indices;
	Posed.GetKeys(Indices);
	Indices.Sort();
	for (const int32 Index : Indices)
	{
		OutBoneTransforms.Add(FBoneTransform(FCompactPoseBoneIndex(Index), Posed[Index]));
	}
}

bool FAnimNode_ProceduralAimOffset::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return BendOrder.Num() > 0 || bCanAim;
}

void FAnimNode_ProceduralAimOffset::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	BendOrder.Reset();
	for (int32 Row = 0; Row < BendBones.Num(); ++Row)
	{
		FAimOffsetBendBone& Bend = BendBones[Row];
		Bend.Bone.Initialize(RequiredBones);
		if (Bend.Bone.IsValidToEvaluate(RequiredBones))
		{
			BendOrder.Add(Row);
		}
	}

	BendOrder.Sort([this, &RequiredBones](int32 A, int32 B)
	{
		return BendBones[A].Bone.GetCompactPoseIndex(RequiredBones) < BendBones[B].Bone.GetCompactPoseIndex(RequiredBones);
	});

	AimPivot.Initialize(RequiredBones);
	AimReference.Initialize(RequiredBones);
	bCanAim = AimPivot.IsValidToEvaluate(RequiredBones)
		&& (AimMode == EAimOffsetAimMode::Remainder || AimReference.IsValidToEvaluate(RequiredBones));

	// The share of the pitch the pivot already gets by being under bent bones; a bent pivot counts its own row too.
	PivotBendShare = 0.0f;
	if (AimPivot.IsValidToEvaluate(RequiredBones))
	{
		const FCompactPoseBoneIndex PivotIndex = AimPivot.GetCompactPoseIndex(RequiredBones);
		for (const int32 Row : BendOrder)
		{
			const FCompactPoseBoneIndex BendIndex = BendBones[Row].Bone.GetCompactPoseIndex(RequiredBones);
			for (FCompactPoseBoneIndex Index = PivotIndex; Index != INDEX_NONE; Index = RequiredBones.GetParentBoneIndex(Index))
			{
				if (Index == BendIndex)
				{
					PivotBendShare += BendBones[Row].Weight;
					break;
				}
			}
		}
	}
}
