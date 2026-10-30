// Round Survival

#pragma once

#include "StateTreeConditionBase.h"

#include "RSSTCondition_BarricadeHasBoards.generated.h"

class ARSZombieController;

USTRUCT()
struct FRSSTCondition_BarricadeHasBoardsInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** Inverts the test, for the branch that wants the window that has come down. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * True while this enemy's window still has boards on it.
 *
 * The switch between tearing and getting in. It reads the barricade's replicated count through
 * the pawn rather than mirroring it anywhere, so a board put back by a player mid-attack is seen
 * on the next tick by every enemy at that window at once — including the ones that had already
 * given up on it.
 *
 * Paired with `Has Entry Barricade` rather than folded into it: one asks whether there is a window
 * to go to at all, this one asks what state that window is in, and the tree branches on them at
 * different depths.
 */
USTRUCT(meta = (DisplayName = "Barricade Has Boards", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTCondition_BarricadeHasBoards : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTCondition_BarricadeHasBoardsInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Condition"); }
#endif // WITH_EDITOR
};
