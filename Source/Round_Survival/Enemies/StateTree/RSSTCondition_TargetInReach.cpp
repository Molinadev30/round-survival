// Round Survival

#include "Enemies/StateTree/RSSTCondition_TargetInReach.h"

#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "Enemies/RSZombieTargetingComponent.h"
#include "Interaction/RSBarricade.h"
#include "StateTreeExecutionContext.h"

bool FRSSTCondition_TargetInReach::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const ARSZombieController* Controller = InstanceData.AIController;
	const ARSZombieCharacter* Enemy = Controller ? Cast<ARSZombieCharacter>(Controller->GetPawn()) : nullptr;
	const ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;
	const URSZombieTargetingComponent* Targeting = Controller ? Controller->GetTargetingComponent() : nullptr;
	const APawn* Target = Targeting ? Targeting->GetTarget() : nullptr;

	FVector Gap;
	if (!Target || !IsValid(Barricade) || !Barricade->GetReachPoint(Controller->BarricadeSlotIndex, Gap))
	{
		return false;
	}

	const FVector TargetLocation = Target->GetActorLocation();

	// Inside the window only: an arm through the boards points inward and nowhere else. Tested
	// against the wall rather than the reach point, which can sit some way into the room.
	if (FVector::DotProduct(TargetLocation - Barricade->GetActorLocation(), Barricade->GetInwardDirection()) <= 0.0)
	{
		return false;
	}

	return (TargetLocation - Gap).Size2D() <= InstanceData.Range;
}
