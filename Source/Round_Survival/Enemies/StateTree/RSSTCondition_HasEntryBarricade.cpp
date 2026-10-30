// Round Survival

#include "Enemies/StateTree/RSSTCondition_HasEntryBarricade.h"

#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "Interaction/RSBarricade.h"
#include "StateTreeExecutionContext.h"

bool FRSSTCondition_HasEntryBarricade::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const ARSZombieController* Controller = InstanceData.AIController;
	const ARSZombieCharacter* Enemy = Controller ? Cast<ARSZombieCharacter>(Controller->GetPawn()) : nullptr;

	// IsValid rather than a null check: a barricade can be torn out of the level between the
	// enemy being handed it and the enemy arriving, and a stale pointer would send it walking to
	// a window that is no longer there.
	const bool bHasBarricade = Enemy && IsValid(Enemy->GetEntryBarricade());

	return bHasBarricade != InstanceData.bInvert;
}
