// Round Survival

#include "AbilitySystem/Tasks/RSAbilityTask_InteractionHold.h"

#include "Abilities/GameplayAbility.h"
#include "GameFramework/Actor.h"
#include "Interaction/RSInteractable.h"
#include "Interaction/RSInteractorComponent.h"

URSAbilityTask_InteractionHold::URSAbilityTask_InteractionHold(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URSAbilityTask_InteractionHold* URSAbilityTask_InteractionHold::InteractionHold(UGameplayAbility* OwningAbility, float Duration,
	AActor* Target, URSInteractorComponent* Interactor)
{
	URSAbilityTask_InteractionHold* Task = NewAbilityTask<URSAbilityTask_InteractionHold>(OwningAbility);

	Task->Duration = FMath::Max(0.0f, Duration);
	Task->Target = Target;
	Task->Interactor = Interactor;

	return Task;
}

bool URSAbilityTask_InteractionHold::IsStillValid() const
{
	AActor* TargetActor = Target.Get();
	const URSInteractorComponent* InteractorComponent = Interactor.Get();

	if (TargetActor == nullptr || InteractorComponent == nullptr)
	{
		return false;
	}

	// Looking away is what ends a hold, and the focus is already being refreshed on the
	// component's own timer — so this is a pointer comparison rather than a second trace.
	if (InteractorComponent->GetFocusedInteractable() != TargetActor)
	{
		return false;
	}

	// A barricade filled by somebody else mid-hold, a locker that just became unaffordable:
	// the target is asked every frame rather than only at the start.
	return IRSInteractable::Execute_CanInteract(TargetActor, GetAvatarActor());
}

void URSAbilityTask_InteractionHold::Activate()
{
	Elapsed = 0.0f;

	// A zero-length hold is an instant interaction and must not wait a frame for its first tick.
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast();
		}

		EndTask();
	}
}

void URSAbilityTask_InteractionHold::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!IsStillValid())
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}

		EndTask();
		return;
	}

	Elapsed += DeltaTime;

	if (Elapsed >= Duration)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnProgress.Broadcast(1.0f);
			OnCompleted.Broadcast();
		}

		EndTask();
		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnProgress.Broadcast(Elapsed / Duration);
	}
}
