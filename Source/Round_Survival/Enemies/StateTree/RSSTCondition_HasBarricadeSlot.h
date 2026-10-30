// Round Survival

#pragma once

#include "StateTreeConditionBase.h"

#include "RSSTCondition_HasBarricadeSlot.generated.h"

class ARSZombieController;

USTRUCT()
struct FRSSTCondition_HasBarricadeSlotInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** Inverts the test, for the branch that wants the enemies still queueing. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * True while this enemy holds a place at its window rather than waiting for one.
 *
 * A condition of its own rather than the engine's bool compare bound to the controller's flag,
 * which is what this started as: a state whose entry test fails leaves the completed state before
 * it with nowhere to go, and a tree that re-selects from the root every frame releases and
 * re-claims the slot every frame with it. Reading the flag through the context object the way
 * every other condition here does takes that whole class of failure off the table.
 */
USTRUCT(meta = (DisplayName = "Has Barricade Slot", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTCondition_HasBarricadeSlot : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTCondition_HasBarricadeSlotInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Condition"); }
#endif // WITH_EDITOR
};
