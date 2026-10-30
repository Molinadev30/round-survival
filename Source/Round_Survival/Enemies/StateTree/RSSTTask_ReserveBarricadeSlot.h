// Round Survival

#pragma once

#include "Tasks/StateTreeAITask.h"

#include "RSSTTask_ReserveBarricadeSlot.generated.h"

class ARSBarricade;
class ARSZombieController;

USTRUCT()
struct FRSSTTask_ReserveBarricadeSlotInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/**
	 * How far outside the window those without a place stand.
	 *
	 * Zero would put the crowd on the window itself, which is where the slots are — the enemies
	 * waiting would then be standing in the spots the attackers are trying to walk into, and push
	 * them out of their own places. The crowd belongs behind the attackers.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float QueueStandoff = 250.0f;

	/**
	 * How close to the window an enemy has to be before it may claim a place at all.
	 *
	 * A place is not something to book from across the map. Without this, an enemy that cannot
	 * reach the window — stuck, shoved, pathing the long way round — would be on the queue, and the
	 * nearest to a freed place could be one that cannot get to it. The queue is for those who are
	 * there; everyone else walks closer, and joins when they arrive.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float SlotClaimDistance = 450.0f;

	/**
	 * How far from its own place an enemy has to be before that place counts as unreachable.
	 *
	 * Standing in the slot, tearing at it, and shuffling around it are all well inside this. Being
	 * this far out means something is in the way — usually the crowd behind.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float SlotAbandonDistance = 150.0f;

	/**
	 * How long it has to stay out of reach before the place is handed back to the queue. The enemy
	 * that gives it up joins the queue as one more, with no wait: the window hands the place to
	 * whoever queued is nearest it, which is not the one that could not get there.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = s))
	float SlotAbandonSeconds = 5.0f;

	/** The window being queued at, kept so the slot goes back to the same one it came from. */
	UPROPERTY()
	TObjectPtr<ARSBarricade> Barricade = nullptr;

	/** The slot held as of the last update, or -1: what a change is told apart from. */
	UPROPERTY()
	int32 SlotIndex = -1;

	/** Whether somebody else held the crossing as of the last update, so its freeing can be announced. */
	UPROPERTY()
	bool bTransitHeldByOther = false;

	/** When this enemy first found its own place out of reach, or zero while it is reachable. */
	UPROPERTY()
	double UnreachableSince = 0.0;
};

/**
 * Queues this enemy for a place at the window it came in through, holds the place the window
 * hands it for as long as the state is active, and tells the tree when it gets one or loses it.
 *
 * The enemy is not looking for a barricade: the spawn point that released it handed it one, so
 * this reads `GetEntryBarricade` and never searches. Within `SlotClaimDistance` it joins the
 * window's queue, and the window (ARSBarricade) hands places out: each free one to the queued
 * enemy nearest it, the moment it frees. This task does not ask for a place; it reads which one
 * the window says it holds.
 *
 * **The tree follows the place, by event.** When the answer changes, this sends
 * `RS.Event.Barricade.SlotGranted` or `RS.Event.Barricade.SlotLost`, and the states under it
 * transition on those: the queue to the walk in, the walk in and the work at the window back to
 * the queue. Nothing polls a flag on tick to find out, and no state carries on after the place it
 * was entered for is gone. It also sends `RS.Event.Barricade.TransitFreed` when somebody else
 * stops holding the crossing, which is what the wait at a breached window climbs on.
 *
 * Where to stand and whether a place is held are written onto the controller rather than kept
 * here: a State Tree binds a task to a context object, not to the task beside it. The place and
 * the queue's spot are two values, `BarricadeSlotLocation` and `BarricadeQueueLocation`, each
 * bound by the one state that walks to it.
 *
 * The slot goes back on the way out, however the state was left — torn down, killed, or the
 * boards ran out — because a slot still held by a corpse is a place at the window nobody can use.
 */
USTRUCT(meta = (DisplayName = "Reserve Barricade Slot", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTTask_ReserveBarricadeSlot : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTTask_ReserveBarricadeSlotInstanceData;

	FRSSTTask_ReserveBarricadeSlot();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
#endif // WITH_EDITOR

protected:
	/** One attempt at holding a place, and the read-out that goes with whichever way it went. */
	EStateTreeRunStatus UpdateReservation(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData) const;
};
