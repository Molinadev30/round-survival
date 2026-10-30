// Round Survival

#pragma once

#include "Tasks/StateTreeAITask.h"

#include "RSSTTask_MoveToTarget.generated.h"

class APawn;
class ARSZombieController;

USTRUCT()
struct FRSSTTask_MoveToTargetInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/**
	 * How close the *walk* tries to get, which is not the same thing as how close the enemy has to
	 * be to attack.
	 *
	 * Kept tight, near enough to touch, so the path following component keeps working until the
	 * enemy is genuinely beside its target. Stopping the walk at attack range instead would strand
	 * an enemy a metre from a player on the far side of a wall, because a straight line calls that
	 * arrived where the real route is all the way around. The attack state interrupts this walk
	 * long before it gets this close; FRSSTCondition_TargetInRange is what decides when.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float AcceptanceRadius = 70.0f;

	/** Minimum time between move requests, so a failing path is not re-asked every frame. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = s))
	float RepathInterval = 0.5f;

	/** Walk as far as the navigation data allows when the target cannot be fully reached. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bAllowPartialPath = true;

	/** The target the current move request was issued for, so a swap re-issues it. */
	UPROPERTY()
	TObjectPtr<APawn> RequestedTarget = nullptr;

	/** Earliest world time another move request may be made. */
	UPROPERTY()
	double NextRequestTime = 0.0;
};

/**
 * Walks the enemy to whatever its targeting component currently points at.
 *
 * It reads the target rather than choosing one: which player to chase is decided on a staggered
 * timer in URSZombieTargetingComponent, and this task reacts when that answer changes.
 *
 * It stays Running for as long as the state is active, both while it has a target and while it
 * has none. Arriving is not this task's business: the attack state takes over on a condition of
 * its own, and that is what stops the enemy where it can actually reach. A chase that ended
 * itself on arrival would need a transition back the moment the player stepped away, and the tree
 * would flip between two states at walking speed.
 *
 * Movement goes through AAIController::MoveToActor and the path following component directly,
 * not through UAITask_MoveTo. The gameplay task version allocates a UObject per move and per
 * agent; with sixty-four enemies re-pathing at walking pace that is a lot of garbage for an
 * indirection we do not use.
 */
USTRUCT(meta = (DisplayName = "Move To Target", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTTask_MoveToTarget : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTTask_MoveToTargetInstanceData;

	FRSSTTask_MoveToTarget();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Movement"); }
#endif // WITH_EDITOR

protected:
	/** One step of the chase: read the target and keep a move request pointed at it. */
	EStateTreeRunStatus UpdateMovement(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData) const;
};
