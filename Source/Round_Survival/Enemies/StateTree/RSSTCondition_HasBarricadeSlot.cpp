// Round Survival

#include "Enemies/StateTree/RSSTCondition_HasBarricadeSlot.h"

#include "Enemies/RSZombieController.h"
#include "StateTreeExecutionContext.h"

bool FRSSTCondition_HasBarricadeSlot::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const ARSZombieController* Controller = InstanceData.AIController;
	const bool bHasSlot = Controller && Controller->bHasBarricadeSlot;

	return bHasSlot != InstanceData.bInvert;
}
