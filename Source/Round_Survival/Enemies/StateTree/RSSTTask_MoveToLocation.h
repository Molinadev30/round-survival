// Round Survival

#pragma once

#include "Tasks/StateTreeAITask.h"

#include "RSSTTask_MoveToLocation.generated.h"

class ARSZombieController;

USTRUCT()
struct FRSSTTask_MoveToLocationInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** Where to walk. Bound from whatever decided it — a barricade slot, a vault entry point. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Destination = FVector::ZeroVector;

	/** How close the walk tries to get before the path following component calls it arrived. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float AcceptanceRadius = 40.0f;

	/** Minimum time between move requests, so a failing path is not re-asked every frame. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = s))
	float RepathInterval = 0.5f;

	/**
	 * How far the destination has to move before the walk is re-issued.
	 *
	 * A slot that shifts by a centimetre is the same slot. Without this, a destination that is
	 * recomputed every tick would throw away a good path on floating point noise alone.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float DestinationTolerance = 25.0f;

	/** Walk as far as the navigation data allows when the destination cannot be fully reached. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bAllowPartialPath = true;

	/**
	 * Inside this distance of the destination, walk straight at it instead of pathfinding.
	 *
	 * For a destination the navmesh does not reach. A barricade slot sits against the wall, and
	 * the navmesh stops an agent radius and a cell short of any wall, so a pathed walk ends on
	 * the mesh's edge and never on the slot. The last stretch is open ground by construction — it
	 * is the strip in front of the window — so a straight line is safe there. Zero turns it off.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float DirectApproachRadius = 0.0f;

	/**
	 * Whether arriving ends the task, or the task is simply "keep standing there".
	 *
	 * Off by default, which is the walk to a window: the destination moves when a place at it
	 * frees up, and a task that ended on arrival would hand the state back every time a queueing
	 * enemy reached the crowd. On, it is a step in a sequence — walk into the slot, then swing —
	 * and the state it sits on completes the moment the pawn is inside `AcceptanceRadius`.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bSucceedOnArrival = false;

	/**
	 * How close counts as arrived, when arriving is what ends the task.
	 *
	 * Deliberately larger than `AcceptanceRadius`: path following stops the pawn *at* that radius,
	 * so testing arrival against the same number sits exactly on the boundary and comes down to
	 * floating point. It also has to survive a crowd — an enemy held a few centimetres out by the
	 * bodies around it has arrived as far as the game is concerned.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float ArrivalRadius = 90.0f;

	/**
	 * Inside this, the pawn has arrived whatever the path following component still believes.
	 *
	 * The component only reports Idle once it is satisfied, and in a crowd it never is: a body
	 * held six centimetres outside its acceptance radius by the bodies beside it stays Moving for
	 * ever, which would mean an enemy that held a place at a window and never
	 * used it, with the queue behind it locked out of a slot nobody was working.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float CloseEnoughRadius = 60.0f;

	/**
	 * Inside `ArrivalRadius`, a walk that has stopped getting closer for this long has arrived.
	 *
	 * The guarantee the two radii above cannot give on their own: a walk ends. `CloseEnoughRadius`
	 * has to be small where places are close together, and path following going Idle is the
	 * component's word, which a straight final approach held a few centimetres short never gives:
	 * it stays Moving with no acceleration for as long as anyone waits. Measured progress is the
	 * pawn's word, and it settles it either way. Zero turns it off.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = s))
	float StallSeconds = 0.5f;

	/** The destination the current request was issued for, so a real move re-issues it. */
	UPROPERTY()
	FVector RequestedDestination = FVector::ZeroVector;

	UPROPERTY()
	bool bHasRequest = false;

	/** Whether the current request is the straight final approach rather than a pathed walk. */
	UPROPERTY()
	bool bRequestIsDirect = false;

	/** Earliest world time another move request may be made. */
	UPROPERTY()
	double NextRequestTime = 0.0;

	/** The nearest the pawn has been to the destination since the walk was asked for, and when it last got nearer. */
	UPROPERTY()
	float ClosestToGo = 0.0f;

	UPROPERTY()
	double LastProgressTime = 0.0;
};

/**
 * Walks the enemy to a point somebody else chose.
 *
 * The counterpart to `FRSSTTask_MoveToTarget`, which follows a pawn the targeting component
 * picked. This one follows a bound location, so the state that owns it decides where: the
 * barricade slot task writes one, and a vault entry point is another. Keeping the choosing out of
 * the walking is what lets one task serve both without either knowing what a window is.
 *
 * By default it stays Running for as long as the state is active, arrived or not: a walk whose
 * destination shifts — a place at a window freeing up — would otherwise end and be re-entered every
 * time it moved, and the state would flicker. `bSucceedOnArrival` turns it into a step in a
 * sequence instead, for a state that walks somewhere and then does something there.
 */
USTRUCT(meta = (DisplayName = "Move To Location", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTTask_MoveToLocation : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTTask_MoveToLocationInstanceData;

	FRSSTTask_MoveToLocation();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Movement"); }
#endif // WITH_EDITOR

protected:
	/** One step of the walk: keep a move request pointed at the bound destination. */
	EStateTreeRunStatus UpdateMovement(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData) const;
};
