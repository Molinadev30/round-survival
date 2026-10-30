// Round Survival

#include "AbilitySystem/RSAbilitySystemComponent.h"

#include "AbilitySystem/Abilities/RSGameplayAbility.h"
#include "RSGameplayTags.h"

URSAbilitySystemComponent::URSAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URSAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	check(ActorInfo);

	const AActor* PreviousAvatar = ActorInfo->AvatarActor.Get();

	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	// Only run the spawn pass when the avatar actually changed. InitAbilityActorInfo is called
	// more than once during possession and on every respawn, and re-activating passives on an
	// unchanged avatar would stack them.
	if (PreviousAvatar != InAvatarActor && InAvatarActor != nullptr)
	{
		TryActivateAbilitiesOnSpawn();
	}
}

void URSAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	ABILITYLIST_SCOPE_LOCK();

	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		const URSGameplayAbility* Ability = Cast<URSGameplayAbility>(Spec.Ability);
		if (Ability == nullptr)
		{
			continue;
		}

		if (Ability->GetActivationPolicy() != ERSAbilityActivationPolicy::OnSpawn || Spec.IsActive())
		{
			continue;
		}

		// Each machine starts only what runs on it. A client asking for a server-only passive would
		// send a request the server refuses, since the server has already started it.
		const EGameplayAbilityNetExecutionPolicy::Type NetPolicy = Ability->GetNetExecutionPolicy();
		const bool bServerExecution = NetPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly
			|| NetPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated;
		const bool bLocalExecution = NetPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly
			|| NetPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted;

		if ((bServerExecution && AbilityActorInfo->IsNetAuthority()) || (bLocalExecution && AbilityActorInfo->IsLocallyControlled()))
		{
			TryActivateAbility(Spec.Handle);
		}
	}
}

void URSAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputPressedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.AddUnique(Spec.Handle);
		}
	}
}

void URSAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.Remove(Spec.Handle);
		}
	}
}

void URSAbilitySystemComponent::ProcessAbilityInput(float DeltaSeconds, bool bGamePaused)
{
	if (HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		// A dead pawn keeps receiving input until the controller lets go of it. Dropping the
		// buffer here rather than checking in every ability keeps the rule in one place.
		ClearAbilityInput();
		return;
	}

	static TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	AbilitiesToActivate.Reset();

	// Held first: an ability that wants to run for as long as the input is down gets a chance
	// every frame, not just on the frame the key went down.
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(SpecHandle);
		if (Spec == nullptr || Spec->Ability == nullptr || Spec->IsActive())
		{
			continue;
		}

		const URSGameplayAbility* Ability = Cast<URSGameplayAbility>(Spec->Ability);
		if (Ability && Ability->ShouldActivateWhileInputHeld(*Spec, AbilityActorInfo.Get()))
		{
			AbilitiesToActivate.AddUnique(Spec->Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(SpecHandle);
		if (Spec == nullptr || Spec->Ability == nullptr)
		{
			continue;
		}

		Spec->InputPressed = true;

		if (Spec->IsActive())
		{
			// Already running: the press is an event for the ability to react to, not a
			// second activation. A reload interrupting itself is not a thing.
			AbilitySpecInputPressed(*Spec);
		}
		else
		{
			const URSGameplayAbility* Ability = Cast<URSGameplayAbility>(Spec->Ability);
			if (Ability && Ability->GetActivationPolicy() == ERSAbilityActivationPolicy::OnInputTriggered)
			{
				AbilitiesToActivate.AddUnique(Spec->Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& SpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(SpecHandle);
	}

	// Releases last, so that a press and a release in the same frame still activate the ability
	// and then immediately tell it the input is gone, rather than dropping the shot entirely.
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(SpecHandle);
		if (Spec == nullptr || Spec->Ability == nullptr)
		{
			continue;
		}

		Spec->InputPressed = false;

		if (Spec->IsActive())
		{
			AbilitySpecInputReleased(*Spec);
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void URSAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

void URSAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	// Replicate the press to the server as a generic event so a running instanced ability can
	// wait on it via WaitInputPress. Super only sets the flag, which an active ability never reads.
	// Only instanced abilities are supported. The per-spec ActivationInfo is the deprecated
	// non-instanced path and is deliberately not consulted.
	const UGameplayAbility* Instance = Spec.IsActive() ? Spec.GetPrimaryInstance() : nullptr;
	if (Instance)
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle,
			Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
}

void URSAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	// Only instanced abilities are supported. The per-spec ActivationInfo is the deprecated
	// non-instanced path and is deliberately not consulted.
	const UGameplayAbility* Instance = Spec.IsActive() ? Spec.GetPrimaryInstance() : nullptr;
	if (Instance)
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
			Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
}
