// Round Survival

#include "Enemies/StateTree/RSSTTask_ReserveBarricadeSlot.h"

#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "Engine/World.h"
#include "Interaction/RSBarricade.h"
#include "RSGameplayTags.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

FRSSTTask_ReserveBarricadeSlot::FRSSTTask_ReserveBarricadeSlot()
{
	// Ticks for the one thing nothing else can tell it: whether its own place has stayed in reach.
	// A distance check, and a lookup in the window's three places.
	bShouldCallTick = true;
}

EStateTreeRunStatus FRSSTTask_ReserveBarricadeSlot::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Instance data survives a re-entry into the same state, so last visit's slot has to be
	// forgotten here or the release on the way out would hand back a slot somebody else now holds.
	InstanceData.Barricade = nullptr;
	InstanceData.SlotIndex = INDEX_NONE;
	InstanceData.UnreachableSince = 0.0;
	InstanceData.bTransitHeldByOther = false;

	return UpdateReservation(Context, InstanceData);
}

EStateTreeRunStatus FRSSTTask_ReserveBarricadeSlot::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return UpdateReservation(Context, Context.GetInstanceData(*this));
}

void FRSSTTask_ReserveBarricadeSlot::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// However this state ended, the place at the window goes back and the queue lets go. A slot held
	// by something that is no longer standing in it is a gap in the queue nobody can fill.
	if (InstanceData.Barricade)
	{
		AActor* Claimant = InstanceData.AIController ? Cast<AActor>(InstanceData.AIController->GetPawn()) : nullptr;
		if (InstanceData.SlotIndex != INDEX_NONE)
		{
			InstanceData.Barricade->ReleaseSlot(InstanceData.SlotIndex, Claimant);
		}
		InstanceData.Barricade->LeaveQueue(Claimant);
	}

	InstanceData.Barricade = nullptr;
	InstanceData.SlotIndex = INDEX_NONE;

	if (InstanceData.AIController)
	{
		InstanceData.AIController->bHasBarricadeSlot = false;
		InstanceData.AIController->BarricadeSlotIndex = INDEX_NONE;
		InstanceData.AIController->BarricadeSlotReachYaw = 0.0f;
	}
}

EStateTreeRunStatus FRSSTTask_ReserveBarricadeSlot::UpdateReservation(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData) const
{
	ARSZombieController* Controller = InstanceData.AIController;
	if (!Controller)
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("Reserve Barricade Slot has no AI controller bound."));
		return EStateTreeRunStatus::Failed;
	}

	ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(Controller->GetPawn());
	ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	if (!Enemy || !IsValid(Barricade))
	{
		// No window to go to. Not an error: most enemies in a round walk in through an opening
		// that was already clear, and the tree's other branch is where they belong.
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Barricade = Barricade;

	const double TimeNow = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.0;
	int32 HeldSlot = Barricade->GetSlotHeldBy(Enemy);

	// A place held by somebody who cannot get to it is worse than a place left empty: the queue
	// behind it is locked out of a slot nobody is working, and the window comes down at a third of
	// the rate it should. Held out of reach for long enough, it goes back, and the window hands it
	// to the nearest of the queue.
	if (HeldSlot != INDEX_NONE)
	{
		const float ToSlot = FVector::Dist2D(Enemy->GetActorLocation(), Barricade->GetSlotTransform(HeldSlot).GetLocation());
		if (ToSlot <= InstanceData.SlotAbandonDistance)
		{
			InstanceData.UnreachableSince = 0.0;
		}
		else if (InstanceData.UnreachableSince <= 0.0)
		{
			InstanceData.UnreachableSince = TimeNow;
		}
		else if (TimeNow - InstanceData.UnreachableSince >= InstanceData.SlotAbandonSeconds)
		{
			Barricade->ReleaseSlot(HeldSlot, Enemy);
			HeldSlot = INDEX_NONE;
			InstanceData.UnreachableSince = 0.0;
		}
	}
	else
	{
		InstanceData.UnreachableSince = 0.0;
	}

	// Without a place, on the queue once close enough to be one of the crowd at the window. Joining
	// is idempotent, and a free place comes back from the join itself if this enemy is the nearest
	// to it. One that gave its place up a moment ago is one more of the queue, with no wait.
	if (HeldSlot == INDEX_NONE)
	{
		if (FVector::Dist2D(Enemy->GetActorLocation(), Barricade->GetActorLocation()) <= InstanceData.SlotClaimDistance)
		{
			Barricade->JoinQueue(Enemy);
			HeldSlot = Barricade->GetSlotHeldBy(Enemy);
		}
		else
		{
			Barricade->LeaveQueue(Enemy);
		}
	}

	const bool bHadSlot = InstanceData.SlotIndex != INDEX_NONE;
	const bool bHasSlot = HeldSlot != INDEX_NONE;
	InstanceData.SlotIndex = HeldSlot;

	Controller->bHasBarricadeSlot = bHasSlot;
	Controller->BarricadeSlotIndex = HeldSlot;
	Controller->BarricadeSlotReachYaw = Barricade->GetReachYawOffset(HeldSlot);
	if (bHasSlot)
	{
		Controller->BarricadeSlotLocation = Barricade->GetSlotTransform(HeldSlot).GetLocation();
	}
	Controller->BarricadeQueueLocation = Barricade->GetActorLocation() + Barricade->GetActorForwardVector() * InstanceData.QueueStandoff;

	// What to face, which is the window and not the place at it. Written every update rather than
	// once, because a barricade is an actor and nothing here promises it never moves.
	Controller->BarricadeLocation = Barricade->GetActorLocation();

	// The tree is told, not left to find out: the queue walks in on one, and whatever was walking
	// to or working the place goes back to the queue on the other.
	if (bHasSlot != bHadSlot)
	{
		Context.SendEvent(bHasSlot ? RSGameplayTags::Event_Barricade_SlotGranted : RSGameplayTags::Event_Barricade_SlotLost);
	}

	// The same for the crossing at a breached window: an enemy waiting at its place for the one
	// climbing through climbs when it is told the way is clear.
	const bool bTransitHeldByOther = Barricade->IsTransitHeldByOther(Enemy);
	if (InstanceData.bTransitHeldByOther && !bTransitHeldByOther)
	{
		Context.SendEvent(RSGameplayTags::Event_Barricade_TransitFreed);
	}
	InstanceData.bTransitHeldByOther = bTransitHeldByOther;

	return EStateTreeRunStatus::Running;
}
