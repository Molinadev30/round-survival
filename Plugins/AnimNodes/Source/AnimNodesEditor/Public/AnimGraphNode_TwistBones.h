#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "AnimNode_TwistBones.h"

#include "AnimGraphNode_TwistBones.generated.h"

#define UE_API ANIMNODESEDITOR_API

/**
 * UAnimGraphNode_TwistBones
 *
 * The anim graph's face of FAnimNode_TwistBones. Compiling checks every row against the target skeleton, so a setup
 * carried to a skeleton without those bones says so instead of silently posing nothing.
 */
UCLASS(MinimalAPI, meta = (Keywords = "Twist Corrective Helper Bones"))
class UAnimGraphNode_TwistBones : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_TwistBones Node;

public:
	//~UEdGraphNode interface
	UE_API virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	UE_API virtual FText GetTooltipText() const override;
	//~End of UEdGraphNode interface

protected:
	//~UAnimGraphNode_SkeletalControlBase interface
	UE_API virtual FText GetControllerDescription() const override;
	UE_API virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
	//~End of UAnimGraphNode_SkeletalControlBase interface
};

#undef UE_API
