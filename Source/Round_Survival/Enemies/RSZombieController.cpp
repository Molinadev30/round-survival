// Round Survival

#include "Enemies/RSZombieController.h"

#include "Components/StateTreeAIComponent.h"
#include "Enemies/RSZombieTargetingComponent.h"
#include "RSLogChannels.h"

ARSZombieController::ARSZombieController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));

	// Not on begin play: an AI controller exists before it has a pawn, and a tree whose tasks
	// address a pawn that is not there yet spends its first frames failing. Possession is the
	// moment the brain has a body, and it is also the moment the pool hands one back.
	StateTreeComponent->SetStartLogicAutomatically(false);

	TargetingComponent = CreateDefaultSubobject<URSZombieTargetingComponent>(TEXT("Targeting"));

	// Enemies see through walls by design: this is a horde that always knows where you are, not a
	// stealth game. Perception would be 64 sense updates a frame buying behaviour we do not want.
	bWantsPlayerState = false;
}

void ARSZombieController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// A pooled body comes back with the brain it had, so whatever the tree wrote about its last
	// window goes before the tree starts. The tasks that wrote it let go of the window itself as
	// they exited, on the unpossess.
	BarricadeSlotLocation = FVector::ZeroVector;
	BarricadeQueueLocation = FVector::ZeroVector;
	bHasBarricadeSlot = false;
	BarricadeSlotIndex = INDEX_NONE;
	BarricadeSlotReachYaw = 0.0f;
	BarricadeLocation = FVector::ZeroVector;
	VaultStartLocation = FVector::ZeroVector;
	LastPlayedAbilityEndTime = -UE_DOUBLE_BIG_NUMBER;

	TargetingComponent->StartTargeting();
	StateTreeComponent->StartLogic();

	UE_LOG(LogRSRounds, Verbose, TEXT("[%s] AI started on %s"), *GetName(), *GetNameSafe(InPawn));
}

void ARSZombieController::OnUnPossess()
{
	// Death and the pool reach us here: the pawn lets go of its controller and keeps it.
	StateTreeComponent->StopLogic(TEXT("Unpossessed"));
	TargetingComponent->StopTargeting();

	Super::OnUnPossess();
}
