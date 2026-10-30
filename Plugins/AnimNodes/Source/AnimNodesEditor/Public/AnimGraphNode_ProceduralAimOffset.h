#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "AnimNode_ProceduralAimOffset.h"

#include "AnimGraphNode_ProceduralAimOffset.generated.h"

#define UE_API ANIMNODESEDITOR_API

/**
 * UAnimGraphNode_ProceduralAimOffset
 *
 * The anim graph's face of FAnimNode_ProceduralAimOffset. Compiling checks the bones against the target skeleton, and
 * that the aim reference is the pivot or under it: turning a pivot cannot move a bone outside its own hierarchy.
 */
UCLASS(MinimalAPI, meta = (Keywords = "Aim Offset Pitch Look Procedural"))
class UAnimGraphNode_ProceduralAimOffset : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_ProceduralAimOffset Node;

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
