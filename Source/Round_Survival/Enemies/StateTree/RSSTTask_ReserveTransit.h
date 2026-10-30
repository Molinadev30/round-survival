// Round Survival

#pragma once

#include "Tasks/StateTreeAITask.h"

#include "RSSTTask_ReserveTransit.generated.h"

class ARSBarricade;
class ARSZombieController;

USTRUCT()
struct FRSSTTask_ReserveTransitInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** The window the claim was taken on, so it goes back to the same one. */
	UPROPERTY()
	TObjectPtr<ARSBarricade> Barricade = nullptr;

	/**
	 * Who took the claim. Kept rather than read back off the controller on the way out, because
	 * the way out is often death, and by then the controller has let go of the pawn — a pooled
	 * body is not destroyed, so a claim it still held would shut the window for good.
	 */
	UPROPERTY()
	TObjectPtr<AActor> Claimant = nullptr;
};

/**
 * Takes the crossing at this enemy's window for as long as the state is active.
 *
 * The climb starts with a step back onto the enemy's own slot — each slot climbs through its own
 * lane — and the step is already part of the crossing: one enemy at a time squares up and goes
 * over, and the rest keep their places at the window until it is through.
 *
 * **No slot, no crossing.** An enemy without a place at the window fails here and goes back to the
 * queue. There is no climbing from wherever it stands: the step up would have nothing to square up
 * to, and the warp would drag the enemy over the sill from metres out.
 *
 * Where to step to is written onto the controller, like the slot task's destination, because a
 * State Tree binds a task to a context object and not to the task beside it.
 *
 * The vault ability takes the same claim again when it fires — `TryReserveTransit` is idempotent
 * by claimant — and gives it back when it lands. This task gives it back on the way out as well,
 * so a crossing abandoned before the climb ever started does not leave the window shut.
 */
USTRUCT(meta = (DisplayName = "Reserve Transit", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTTask_ReserveTransit : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTTask_ReserveTransitInstanceData;

	FRSSTTask_ReserveTransit();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
#endif // WITH_EDITOR
};
