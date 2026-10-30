#include "AnimGraphNode_TwistBones.h"

#include "Animation/Skeleton.h"
#include "Kismet2/CompilerResultsLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_TwistBones)

#define LOCTEXT_NAMESPACE "AnimGraphNode_TwistBones"

FText UAnimGraphNode_TwistBones::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		return GetControllerDescription();
	}

	return FText::Format(LOCTEXT("TitleWithCount", "{0}\n{1} bones"), GetControllerDescription(), FText::AsNumber(Node.Bones.Num()));
}

FText UAnimGraphNode_TwistBones::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Poses twist bones and corrective roots from the joints the animation moves: a fraction of each source's twist or swing, and a place along a span. One row per helper bone.");
}

FText UAnimGraphNode_TwistBones::GetControllerDescription() const
{
	return LOCTEXT("Description", "Twist Bones");
}

void UAnimGraphNode_TwistBones::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	if (ForSkeleton)
	{
		const FReferenceSkeleton& RefSkeleton = ForSkeleton->GetReferenceSkeleton();
		const auto CheckBone = [this, &RefSkeleton, &MessageLog](const FBoneReference& Reference, int32 Row, const TCHAR* Role)
		{
			if (!Reference.BoneName.IsNone() && RefSkeleton.FindBoneIndex(Reference.BoneName) == INDEX_NONE)
			{
				MessageLog.Warning(*FText::Format(LOCTEXT("MissingBone", "@@ row {0}: {1} bone '{2}' is not on the skeleton."),
					FText::AsNumber(Row), FText::FromString(Role), FText::FromName(Reference.BoneName)).ToString(), this);
			}
		};

		TSet<FName> PosedBones;
		for (int32 Row = 0; Row < Node.Bones.Num(); ++Row)
		{
			const FTwistBoneSetup& Setup = Node.Bones[Row];
			if (Setup.Bone.BoneName.IsNone())
			{
				MessageLog.Warning(*FText::Format(LOCTEXT("NoBone", "@@ row {0} poses no bone."), FText::AsNumber(Row)).ToString(), this);
				continue;
			}

			// Two rows on one bone would write it twice, and the second write would win without saying so.
			bool bAlreadyPosed = false;
			PosedBones.Add(Setup.Bone.BoneName, &bAlreadyPosed);
			if (bAlreadyPosed)
			{
				MessageLog.Warning(*FText::Format(LOCTEXT("DuplicateBone", "@@ row {0}: '{1}' is already posed by an earlier row."),
					FText::AsNumber(Row), FText::FromName(Setup.Bone.BoneName)).ToString(), this);
			}

			CheckBone(Setup.Bone, Row, TEXT("posed"));
			CheckBone(Setup.RotationSource, Row, TEXT("rotation source"));
			CheckBone(Setup.SpanStart, Row, TEXT("span start"));
			CheckBone(Setup.SpanEnd, Row, TEXT("span end"));
		}
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

#undef LOCTEXT_NAMESPACE
