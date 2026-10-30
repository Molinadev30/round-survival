// Round Survival

#include "Enemies/StateTree/RSSTTask_MoveToTarget.h"

#include "Enemies/RSEnemyDebug.h"
#include "Enemies/RSZombieController.h"
#include "Enemies/RSZombieTargetingComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

FRSSTTask_MoveToTarget::FRSSTTask_MoveToTarget()
{
	// The target can change under us at any refresh and the player never stops moving, so this
	// one does need a tick. It is a comparison and a status read, not a path query.
	bShouldCallTick = true;
}

EStateTreeRunStatus FRSSTTask_MoveToTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Instance data survives a re-entry into the same state, so the previous visit's request has
	// to be forgotten or this would believe a move is already under way.
	InstanceData.RequestedTarget = nullptr;
	InstanceData.NextRequestTime = 0.0;

	return UpdateMovement(Context, InstanceData);
}

EStateTreeRunStatus FRSSTTask_MoveToTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return UpdateMovement(Context, Context.GetInstanceData(*this));
}

void FRSSTTask_MoveToTarget::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Whatever comes next owns the pawn's movement. Leaving a request running would have the path
	// following component keep steering underneath the state that replaced this one, and the state
	// that usually replaces this one is the attack, which has to stand still.
	if (InstanceData.AIController && InstanceData.RequestedTarget)
	{
		InstanceData.AIController->StopMovement();
	}

	InstanceData.RequestedTarget = nullptr;
}

EStateTreeRunStatus FRSSTTask_MoveToTarget::UpdateMovement(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData) const
{
	ARSZombieController* Controller = InstanceData.AIController;
	if (!Controller)
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("Move To Target has no AI controller bound."));
		return EStateTreeRunStatus::Failed;
	}

	APawn* ControlledPawn = Controller->GetPawn();
	URSZombieTargetingComponent* Targeting = Controller->GetTargetingComponent();
	if (!ControlledPawn || !Targeting)
	{
		return EStateTreeRunStatus::Failed;
	}

	APawn* Target = Targeting->GetTarget();

	if (!Target)
	{
		// Nobody left to chase: every player dead, or none spawned yet. Stand still and keep the
		// state running, because the targeting component will hand one over when there is one.
		if (InstanceData.RequestedTarget)
		{
			Controller->StopMovement();
			InstanceData.RequestedTarget = nullptr;
		}

		return EStateTreeRunStatus::Running;
	}

	const bool bTargetChanged = InstanceData.RequestedTarget != Target;
	const bool bIdle = Controller->GetMoveStatus() == EPathFollowingStatus::Idle;
	const double TimeNow = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.0;

	RSEnemyDebug::DrawEnemyState(ControlledPawn, Target, TEXT("Chase"), Targeting->GetTargetPathDistance());

	// A move is re-issued when the target changes, and otherwise only once the previous one has
	// ended: MoveToActor follows a moving goal on its own, so re-asking every frame would throw
	// away a perfectly good path sixty-four times a frame. The interval also throttles the retry
	// when there is no path to be had at all.
	const bool bShouldRequest = bTargetChanged || (bIdle && TimeNow >= InstanceData.NextRequestTime);
	if (!bShouldRequest)
	{
		return EStateTreeRunStatus::Running;
	}

	const EPathFollowingRequestResult::Type RequestResult = Controller->MoveToActor(Target, InstanceData.AcceptanceRadius,
		/*bStopOnOverlap=*/ false, /*bUsePathfinding=*/ true, /*bCanStrafe=*/ false,
		/*FilterClass=*/ nullptr, InstanceData.bAllowPartialPath);

	InstanceData.RequestedTarget = Target;
	InstanceData.NextRequestTime = TimeNow + InstanceData.RepathInterval;

	if (RequestResult == EPathFollowingRequestResult::Failed)
	{
		// Unreachable for now: outside the navigation mesh, or on the far side of a door that is
		// still closed. The state stays Running and the interval above throttles the retry; a
		// failure here is a fact about the world this frame, not a reason to leave the chase.
		UE_VLOG(Context.GetOwner(), LogStateTree, Verbose, TEXT("Move To Target could not path to %s."), *GetNameSafe(Target));
	}

	return EStateTreeRunStatus::Running;
}
