#include "AnimGraphNode_ProceduralAimOffset.h"

#include "Animation/Skeleton.h"
#include "Kismet2/CompilerResultsLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_ProceduralAimOffset)

#define LOCTEXT_NAMESPACE "AnimGraphNode_ProceduralAimOffset"

FText UAnimGraphNode_ProceduralAimOffset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FName Aimed = Node.AimMode == EAimOffsetAimMode::Absolute ? Node.AimReference.BoneName : Node.AimPivot.BoneName;
	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle || Aimed.IsNone())
	{
		return GetControllerDescription();
	}

	return FText::Format(LOCTEXT("TitleWithAimed", "{0}\nAims: {1}"), GetControllerDescription(), FText::FromName(Aimed));
}

FText UAnimGraphNode_ProceduralAimOffset::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Pitches the pose to an angle without aim-offset animations: bends a chain by a share of the pitch per bone, then turns a pivot until a reference bone's axis points exactly at the angle.");
}

FText UAnimGraphNode_ProceduralAimOffset::GetControllerDescription() const
{
	return LOCTEXT("Description", "Procedural Aim Offset");
}

void UAnimGraphNode_ProceduralAimOffset::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	if (ForSkeleton)
	{
		const FReferenceSkeleton& RefSkeleton = ForSkeleton->GetReferenceSkeleton();
		const auto CheckBone = [this, &RefSkeleton, &MessageLog](const FBoneReference& Reference, const FText& Role)
		{
			if (!Reference.BoneName.IsNone() && RefSkeleton.FindBoneIndex(Reference.BoneName) == INDEX_NONE)
			{
				MessageLog.Warning(*FText::Format(LOCTEXT("MissingBone", "@@: {0} '{1}' is not on the skeleton."), Role, FText::FromName(Reference.BoneName)).ToString(), this);
			}
		};

		for (int32 Row = 0; Row < Node.BendBones.Num(); ++Row)
		{
			CheckBone(Node.BendBones[Row].Bone, FText::Format(LOCTEXT("BendRole", "bend row {0}"), FText::AsNumber(Row)));
		}
		CheckBone(Node.AimPivot, LOCTEXT("PivotRole", "aim pivot"));
		CheckBone(Node.AimReference, LOCTEXT("ReferenceRole", "aim reference"));

		const bool bHasPivot = !Node.AimPivot.BoneName.IsNone();
		const bool bHasReference = !Node.AimReference.BoneName.IsNone();
		const bool bAbsolute = Node.AimMode == EAimOffsetAimMode::Absolute;
		if (bAbsolute && bHasPivot != bHasReference)
		{
			MessageLog.Warning(*LOCTEXT("HalfAim", "@@ aims in Absolute mode with only one of aim pivot and aim reference: the aim step is skipped.").ToString(), this);
		}
		else if (bAbsolute && bHasPivot)
		{
			const int32 PivotIndex = RefSkeleton.FindBoneIndex(Node.AimPivot.BoneName);
			int32 Index = RefSkeleton.FindBoneIndex(Node.AimReference.BoneName);
			while (Index != INDEX_NONE && Index != PivotIndex)
			{
				Index = RefSkeleton.GetParentIndex(Index);
			}
			if (PivotIndex != INDEX_NONE && Index == INDEX_NONE)
			{
				MessageLog.Warning(*FText::Format(LOCTEXT("ReferenceNotUnderPivot", "@@: aim reference '{0}' is not under aim pivot '{1}', so turning the pivot cannot aim it."),
					FText::FromName(Node.AimReference.BoneName), FText::FromName(Node.AimPivot.BoneName)).ToString(), this);
			}
		}
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

#undef LOCTEXT_NAMESPACE
