// Round Survival

#include "Enemies/StateTree/RSSTTask_MoveToLocation.h"

#include "Enemies/RSZombieController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

namespace RSMoveToLocation
{
	/** Getting nearer by less than this is jostling in place, not progress. */
	static constexpr float ProgressStep = 2.0f;
}

FRSSTTask_MoveToLocation::FRSSTTask_MoveToLocation()
{
	// The bound destination can move under us — a slot is handed over the moment one frees — so
	// this compares and re-issues. It is a distance check, not a path query.
	bShouldCallTick = true;
}

EStateTreeRunStatus FRSSTTask_MoveToLocation::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Instance data survives a re-entry, so the previous visit's request has to be forgotten or
	// this would believe a move is already under way.
	InstanceData.bHasRequest = false;
	InstanceData.bRequestIsDirect = false;
	InstanceData.RequestedDestination = FVector::ZeroVector;
	InstanceData.NextRequestTime = 0.0;
	InstanceData.ClosestToGo = TNumericLimits<float>::Max();
	InstanceData.LastProgressTime = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.0;

	return UpdateMovement(Context, InstanceData);
}

EStateTreeRunStatus FRSSTTask_MoveToLocation::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return UpdateMovement(Context, Context.GetInstanceData(*this));
}

void FRSSTTask_MoveToLocation::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Whatever comes next owns the pawn's movement. The state that usually replaces this one is
	// the attack, which has to stand still.
	if (InstanceData.AIController && InstanceData.bHasRequest)
	{
		InstanceData.AIController->StopMovement();
	}

	InstanceData.bHasRequest = false;
}

EStateTreeRunStatus FRSSTTask_MoveToLocation::UpdateMovement(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData) const
{
	ARSZombieController* Controller = InstanceData.AIController;
	if (!Controller)
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("Move To Location has no AI controller bound."));
		return EStateTreeRunStatus::Failed;
	}

	if (!Controller->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}

	// Close enough for the straight final approach. Crossing that line either way is a reason to
	// re-issue, since the request in flight is the wrong kind of walk for where the pawn now is.
	const bool bWantsDirect = InstanceData.DirectApproachRadius > 0.0f
		&& FVector::Dist2D(Controller->GetPawn()->GetActorLocation(), InstanceData.Destination) <= InstanceData.DirectApproachRadius;

	const bool bMoved = !InstanceData.bHasRequest
		|| bWantsDirect != InstanceData.bRequestIsDirect
		|| FVector::DistSquared(InstanceData.RequestedDestination, InstanceData.Destination)
			> FMath::Square(InstanceData.DestinationTolerance);

	const bool bIdle = Controller->GetMoveStatus() == EPathFollowingStatus::Idle;
	const double TimeNow = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.0;

	// Two ways to have arrived, and it needs both or it breaks in one of two directions. Close
	// enough is arrival on its own, because a pawn wedged in a crowd never satisfies its path
	// following component and would otherwise walk on the spot for ever. Further out, Idle is what
	// says the walk is over — the component reports it for arriving and for giving up alike, and
	// the distance is what tells those apart. Distance alone would end the walk the moment the
	// pawn crossed the line, leaving it swinging from wherever that happened to be.
	// A pathed walk going idle on the navmesh's edge has not arrived if the straight final approach
	// is still to come, so idle only counts once the walk in flight is the kind this spot wants.
	// The third way settles whatever those two leave open: inside the arrival radius, a pawn that
	// has stopped getting closer is there, whatever path following still claims.
	if (InstanceData.bSucceedOnArrival && InstanceData.bHasRequest)
	{
		const float ToGo = FVector::Dist2D(Controller->GetPawn()->GetActorLocation(), InstanceData.Destination);
		if (ToGo < InstanceData.ClosestToGo - RSMoveToLocation::ProgressStep)
		{
			InstanceData.ClosestToGo = ToGo;
			InstanceData.LastProgressTime = TimeNow;
		}

		const bool bIdleArrived = bIdle && bWantsDirect == InstanceData.bRequestIsDirect && ToGo <= InstanceData.ArrivalRadius;
		const bool bStalledArrived = InstanceData.StallSeconds > 0.0f && ToGo <= InstanceData.ArrivalRadius
			&& TimeNow - InstanceData.LastProgressTime >= InstanceData.StallSeconds;
		if (ToGo <= InstanceData.CloseEnoughRadius || bIdleArrived || bStalledArrived)
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	// Re-issued when the destination really moves, and otherwise only once the previous request
	// has ended. Re-asking every frame would throw away a good path sixty-four times a frame.
	const bool bShouldRequest = bMoved || (bIdle && TimeNow >= InstanceData.NextRequestTime);
	if (!bShouldRequest)
	{
		return EStateTreeRunStatus::Running;
	}

	// A straight walk must not be projected either: projecting would pull the destination back
	// onto the navmesh's edge, which is the very spot the straight walk exists to get past.
	const EPathFollowingRequestResult::Type RequestResult = Controller->MoveToLocation(InstanceData.Destination,
		InstanceData.AcceptanceRadius, /*bStopOnOverlap=*/ false, /*bUsePathfinding=*/ !bWantsDirect,
		/*bProjectDestinationToNavigation=*/ !bWantsDirect, /*bCanStrafe=*/ false,
		/*FilterClass=*/ nullptr, InstanceData.bAllowPartialPath);

	// A new walk, or the same walk to a place that moved: progress counts from here.
	if (bMoved)
	{
		InstanceData.ClosestToGo = TNumericLimits<float>::Max();
		InstanceData.LastProgressTime = TimeNow;
	}

	InstanceData.RequestedDestination = InstanceData.Destination;
	InstanceData.bHasRequest = true;
	InstanceData.bRequestIsDirect = bWantsDirect;
	InstanceData.NextRequestTime = TimeNow + InstanceData.RepathInterval;

	if (RequestResult == EPathFollowingRequestResult::Failed)
	{
		// Unreachable this frame: off the navigation mesh, or behind something not open yet. The
		// state stays Running and the interval throttles the retry — a failure here is a fact
		// about the world right now, not a reason to abandon where the enemy was told to go.
		UE_VLOG(Context.GetOwner(), LogStateTree, Verbose, TEXT("Move To Location could not path to %s."),
			*InstanceData.Destination.ToCompactString());
	}

	return EStateTreeRunStatus::Running;
}
