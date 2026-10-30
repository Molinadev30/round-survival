// Round Survival

#pragma once

#include "StateTreeConditionBase.h"

#include "RSSTCondition_TargetInReach.generated.h"

class ARSZombieController;

USTRUCT()
struct FRSSTCondition_TargetInReachInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/**
	 * How close to this slot's gap the target's centre has to be, horizontally.
	 *
	 * Kept a little inside what the reach's sweep can actually touch, so an enemy only commits to
	 * a reach it can land: the decision and the blow measure from the same gap.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float Range = 100.0f;
};

/**
 * True while this enemy's target is within reach of the gap its slot owns, on the inside.
 *
 * Measured from the gap rather than from the enemy — the enemy stands 80 cm out, and a player an
 * arm's length from the boards is well over a metre from it — and only on the far side of the
 * window, so a player walking past outside is not "in reach" of an arm pointing the other way.
 * Walls do not need testing: the gap is in the window, and the window is the one opening there is.
 */
USTRUCT(meta = (DisplayName = "Target In Reach", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTCondition_TargetInReach : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTCondition_TargetInReachInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Condition"); }
#endif // WITH_EDITOR
};
