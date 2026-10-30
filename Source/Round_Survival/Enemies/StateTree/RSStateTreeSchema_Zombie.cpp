// Round Survival

#include "Enemies/StateTree/RSStateTreeSchema_Zombie.h"

#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"

URSStateTreeSchema_Zombie::URSStateTreeSchema_Zombie(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Context slot 0 is the controlled actor and slot 1 the controller, in that order, as the
	// parent schema lays them out. Narrowing the classes is the whole job of this subclass.
	ContextActorClass = ARSZombieCharacter::StaticClass();
	ContextDataDescs[0].Struct = ContextActorClass.Get();

	AIControllerClass = ARSZombieController::StaticClass();
	ContextDataDescs[1].Struct = AIControllerClass.Get();
}
