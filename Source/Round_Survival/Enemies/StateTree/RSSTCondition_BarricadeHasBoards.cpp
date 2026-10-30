// Round Survival

#include "Enemies/StateTree/RSSTCondition_BarricadeHasBoards.h"

#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "Interaction/RSBarricade.h"
#include "StateTreeExecutionContext.h"

bool FRSSTCondition_BarricadeHasBoards::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const ARSZombieController* Controller = InstanceData.AIController;
	const ARSZombieCharacter* Enemy = Controller ? Cast<ARSZombieCharacter>(Controller->GetPawn()) : nullptr;
	const ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	// A window that no longer exists counts as one with nothing left on it, not as an error: the
	// enemy is inside either way, and the branch that handles a breach is the one that should run.
	const bool bHasBoards = IsValid(Barricade) && !Barricade->IsBreached();

	return bHasBoards != InstanceData.bInvert;
}
