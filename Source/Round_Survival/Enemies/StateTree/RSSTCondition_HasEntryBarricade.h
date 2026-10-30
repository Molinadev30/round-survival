// Round Survival

#pragma once

#include "StateTreeConditionBase.h"

#include "RSSTCondition_HasEntryBarricade.generated.h"

class ARSZombieController;

USTRUCT()
struct FRSSTCondition_HasEntryBarricadeInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** Inverts the test, for the branch that wants the enemies who simply walked in. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * True while this enemy still has a window to get in through.
 *
 * The one test that splits the tree in two. An enemy released from a spawn point behind a
 * barricade carries a reference to it and heads there; one released from an open point has none
 * and goes straight for a player. It is also what ends the barricade branch: the traverse clears
 * the reference once the window has been crossed, so the same condition that sent the enemy to
 * the window is what stops sending it back.
 *
 * A function on the pawn rather than a bindable property, hence a condition in C++: a State Tree
 * binds to properties, and making this one bindable would mean mirroring a piece of state that
 * already has an owner.
 */
USTRUCT(meta = (DisplayName = "Has Entry Barricade", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTCondition_HasEntryBarricade : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTCondition_HasEntryBarricadeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Condition"); }
#endif // WITH_EDITOR
};
