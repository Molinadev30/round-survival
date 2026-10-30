// Round Survival

#include "Enemies/StateTree/RSSTCondition_BarricadeTransitFree.h"

#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "Interaction/RSBarricade.h"
#include "StateTreeExecutionContext.h"

bool FRSSTCondition_BarricadeTransitFree::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const ARSZombieController* Controller = InstanceData.AIController;
	const ARSZombieCharacter* Enemy = Controller ? Cast<ARSZombieCharacter>(Controller->GetPawn()) : nullptr;
	const ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	// No window, no crossing to make or to wait for: false either way, so neither the traversal nor
	// the wait for it is entered only to fail for the same reason. A crossing this enemy already
	// holds is free as far as it is concerned — it reserves before stepping up, and re-selects while
	// it walks.
	if (!IsValid(Barricade))
	{
		return false;
	}

	const bool bFree = !Barricade->IsTransitHeldByOther(Enemy);
	return bFree != InstanceData.bInvert;
}
