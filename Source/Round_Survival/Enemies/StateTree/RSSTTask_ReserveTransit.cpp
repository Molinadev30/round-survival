// Round Survival

#include "Enemies/StateTree/RSSTTask_ReserveTransit.h"

#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "Interaction/RSBarricade.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

FRSSTTask_ReserveTransit::FRSSTTask_ReserveTransit()
{
	// Nothing to watch once the claim is held: the children walk and climb, this only holds.
	bShouldCallTick = false;

	// Holding a claim is not something that finishes. The state ends when the climb does.
#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
#endif
}

EStateTreeRunStatus FRSSTTask_ReserveTransit::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Instance data survives a re-entry, so last visit's window has to be forgotten here or the
	// release on the way out would reach for a claim this visit never took.
	InstanceData.Barricade = nullptr;
	InstanceData.Claimant = nullptr;

	ARSZombieController* Controller = InstanceData.AIController;
	if (!Controller)
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("Reserve Transit has no AI controller bound."));
		return EStateTreeRunStatus::Failed;
	}

	ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(Controller->GetPawn());
	ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	// The climb starts from this enemy's own slot, so without one there is nothing to start from.
	// The state's enter condition already asks; failing here as well keeps a tree edited without it
	// from turning a freshly spawned enemy into one that vaults from metres away.
	const int32 SlotIndex = Controller->BarricadeSlotIndex;
	if (SlotIndex == INDEX_NONE)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The state's enter condition asked the same question a moment ago, so losing here means two
	// enemies were selected on the same frame. The loser fails back to the queue and asks again.
	if (!IsValid(Barricade) || !Barricade->TryReserveTransit(Enemy))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Barricade = Barricade;
	InstanceData.Claimant = Enemy;
	// The climb goes over in this slot's lane. The step up is only there to square the enemy back
	// onto its spot if the crowd shoved it off; nobody crosses the window to a shared start point
	// and walks into whoever holds the middle.
	Controller->VaultStartLocation = Barricade->GetSlotTransform(SlotIndex).GetLocation();

	return EStateTreeRunStatus::Running;
}

void FRSSTTask_ReserveTransit::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Only the holder frees it, so this is harmless after the vault has already let go on landing.
	if (InstanceData.Barricade)
	{
		InstanceData.Barricade->ReleaseTransit(InstanceData.Claimant);
	}

	InstanceData.Barricade = nullptr;
	InstanceData.Claimant = nullptr;
}
