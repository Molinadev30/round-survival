// Round Survival

#pragma once

#include "StateTreeConditionBase.h"

#include "RSSTCondition_BarricadeTransitFree.generated.h"

class ARSZombieController;

USTRUCT()
struct FRSSTCondition_BarricadeTransitFreeInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** Inverts the test: somebody else is crossing. For the state that waits its turn at the sill. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * True while nobody else holds the crossing at this enemy's window.
 *
 * The gate on `TraverseWindow`. The vault ability refuses to start while the opening is taken, and
 * a refused ability fails its state — which the tree would read as the whole barricade branch
 * failing and send the enemy to chase a player it has no path to. Asked here first, a busy window
 * is a reason to wait in the queue instead: the state is never entered, and the next fallback runs.
 *
 * The ability keeps its own check. This one decides which state to pick; that one decides whether
 * the climb is allowed, and is the one that holds if two enemies ask on the same frame.
 */
USTRUCT(meta = (DisplayName = "Barricade Transit Free", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTCondition_BarricadeTransitFree : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTCondition_BarricadeTransitFreeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Condition"); }
#endif // WITH_EDITOR
};
