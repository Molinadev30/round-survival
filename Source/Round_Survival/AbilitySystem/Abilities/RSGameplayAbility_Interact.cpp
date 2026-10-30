// Round Survival

#include "AbilitySystem/Abilities/RSGameplayAbility_Interact.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Tasks/RSAbilityTask_InteractionHold.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Actor.h"
#include "Interaction/RSInteractable.h"
#include "Interaction/RSInteractorComponent.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

URSGameplayAbility_Interact::URSGameplayAbility_Interact(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Held, not tapped: the ability lives for as long as the key is down, which is what a hold is.
	ActivationPolicy = ERSAbilityActivationPolicy::WhileInputActive;

	// Held for the whole activation. Animation and the HUD read this rather than asking the
	// interactor component whether something is in progress.
	ActivationOwnedTags.AddTag(RSGameplayTags::State_Interacting);

	// A switch sets the drawn slot on the server partway through. A weapon bought in that window
	// would be drawn and then holstered again by the switch landing on top of it.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_SwitchingWeapon);

	// A downed player buys nothing and repairs nothing. One already holding when they went down is
	// cancelled by the downed ability; this stops the held key starting another.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Downed);

	// Interacting does not take the weapon hands by itself, by design: a player repairs a
	// barricade or opens a gate with the gun still working. The targets that do take them — a revive,
	// a station — say so through InterruptsWeapon, and BeginInteractionStep acts on it.
	WeaponAbilitiesToInterrupt.AddTag(RSGameplayTags::Ability_Fire);
	WeaponAbilitiesToInterrupt.AddTag(RSGameplayTags::Ability_Reload);
	WeaponAbilitiesToInterrupt.AddTag(RSGameplayTags::Ability_ADS);
	WeaponAbilitiesToInterrupt.AddTag(RSGameplayTags::Ability_Sprint);
	WeaponAbilitiesToInterrupt.AddTag(RSGameplayTags::Ability_Melee);

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Interact);
	SetAssetTags(AssetTags);
}

URSInteractorComponent* URSGameplayAbility_Interact::GetInteractorComponent() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? Avatar->FindComponentByClass<URSInteractorComponent>() : nullptr;
}

bool URSGameplayAbility_Interact::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Reading the cached focus rather than tracing: this is asked every frame the key is held
	// with nothing in front of the player, and a trace per frame for an answer the component
	// already has ten times a second would be pure waste.
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	URSInteractorComponent* Interactor = Avatar ? Avatar->FindComponentByClass<URSInteractorComponent>() : nullptr;
	AActor* Focus = Interactor ? Interactor->GetFocusedInteractable() : nullptr;

	if (Focus == nullptr)
	{
		// The client would not have activated without a focus, so on the server an empty cache
		// usually means its timer has not caught up with a player who just walked up to
		// something. One trace settles it; refusing would cancel a legitimate interaction.
		if (Interactor == nullptr || !ActorInfo->IsNetAuthority())
		{
			return false;
		}

		Interactor->RefreshFocus();
		Focus = Interactor->GetFocusedInteractable();

		if (Focus == nullptr)
		{
			return false;
		}
	}

	return IRSInteractable::Execute_CanInteract(Focus, Avatar);
}

void URSGameplayAbility_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Instanced per actor: the previous activation's state is still here.
	CurrentTarget = nullptr;
	bWaitingForRelease = false;

	// Releasing the key is the only thing that ends this ability from the player's side. The
	// press is replicated as a generic event by the ability system component, so this works the
	// same on the client and on the server.
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/ true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
	ReleaseTask->ReadyForActivation();

	BeginInteractionStep();
}

void URSGameplayAbility_Interact::BeginInteractionStep()
{
	URSInteractorComponent* Interactor = GetInteractorComponent();
	AActor* Target = Interactor ? Interactor->GetFocusedInteractable() : nullptr;
	AActor* Avatar = GetAvatarActorFromActorInfo();

	if (Target == nullptr || !IRSInteractable::Execute_CanInteract(Target, Avatar))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	CurrentTarget = Target;

	if (IRSInteractable::Execute_InterruptsWeapon(Target, Avatar))
	{
		TakeHands();
	}

	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		HeldTarget = Target;
		IRSInteractable::Execute_OnInteractionHoldStarted(Target, Avatar);
	}

	// The target owns the duration, not the ability: a barricade board and a locker purchase are
	// the same code with different numbers in data.
	const float HoldSeconds = IRSInteractable::Execute_GetHoldSeconds(Target, Avatar);

	URSAbilityTask_InteractionHold* HoldTask = URSAbilityTask_InteractionHold::InteractionHold(this, HoldSeconds, Target, Interactor);
	HoldTask->OnProgress.AddDynamic(this, &ThisClass::OnHoldProgress);
	HoldTask->OnCompleted.AddDynamic(this, &ThisClass::OnHoldCompleted);
	HoldTask->OnCancelled.AddDynamic(this, &ThisClass::OnHoldCancelled);
	HoldTask->ReadyForActivation();
}

void URSGameplayAbility_Interact::OnHoldProgress(float Progress)
{
	if (URSInteractorComponent* Interactor = GetInteractorComponent())
	{
		Interactor->SetHoldProgress(Progress);
	}
}

void URSGameplayAbility_Interact::OnHoldCompleted()
{
	CompleteInteractionStep();
}

void URSGameplayAbility_Interact::OnHoldCancelled()
{
	// Looked away, walked out of range, or the target stopped accepting it. Nothing was paid and
	// nothing happened: the hold is simply over.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
}

void URSGameplayAbility_Interact::CompleteInteractionStep()
{
	AActor* Target = CurrentTarget.Get();
	AActor* Avatar = GetAvatarActorFromActorInfo();

	if (Target == nullptr)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	if (URSInteractorComponent* Interactor = GetInteractorComponent())
	{
		Interactor->SetHoldProgress(0.0f);
	}

	// Before Interact, so the target sees the hold end whether or not the interaction goes through.
	EndHold();

	// Paid here, not on the press. A purchase charged at the start of a hold would take the scrap
	// from a player who then let go, and hand them nothing. The cost reads its price from the
	// target this ability is holding, which is the one that has just been validated.
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogRS, Verbose, TEXT("[%s] could not pay for [%s]."), *GetNameSafe(Avatar), *GetNameSafe(Target));

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	// The client ran the hold for the timing and the HUD; what actually happens happens here,
	// once, on the server.
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		UE_LOG(LogRS, Verbose, TEXT("[%s] interacted with [%s]."), *GetNameSafe(Avatar), *GetNameSafe(Target));

		IRSInteractable::Execute_Interact(Target, Avatar);
	}

	// A repeatable target that still has something left to give goes straight into another hold:
	// one press, board after board, each one paying as it lands.
	if (IRSInteractable::Execute_IsRepeatable(Target) && IRSInteractable::Execute_CanInteract(Target, Avatar))
	{
		BeginInteractionStep();
		return;
	}

	// Otherwise the ability stays alive, doing nothing, until the key comes up. Ending here
	// instead would let the activation policy re-activate it on the next frame and buy twice.
	bWaitingForRelease = true;
}

void URSGameplayAbility_Interact::OnInputReleased(float TimeHeld)
{
	// A release mid-hold is a cancel; a release after the interaction is just the end of it.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ !bWaitingForRelease);
}

void URSGameplayAbility_Interact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// However this ended — released, cancelled, dead — the ring on the HUD stops filling.
	if (URSInteractorComponent* Interactor = GetInteractorComponent())
	{
		Interactor->SetHoldProgress(0.0f);
	}

	// A hold let go or cut short never reached CompleteInteractionStep.
	EndHold();

	CurrentTarget = nullptr;
	bWaitingForRelease = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_Interact::TakeHands()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr)
	{
		return;
	}

	// A burst or a reload under way stops; the tag keeps either from starting again until the hold is
	// over. Both machines that run this ability do it, so the client predicts it and the server
	// agrees.
	ASC->CancelAbilities(&WeaponAbilitiesToInterrupt);

	if (!bHandsTaken)
	{
		// Loose, not replicated: only the two machines running this ability gate on it, and each one
		// puts it on and takes it off itself.
		ASC->AddLooseGameplayTag(RSGameplayTags::State_HandsBusy);
		bHandsTaken = true;
	}
}

void URSGameplayAbility_Interact::ReleaseHands()
{
	if (!bHandsTaken)
	{
		return;
	}

	bHandsTaken = false;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(RSGameplayTags::State_HandsBusy);
	}
}

void URSGameplayAbility_Interact::EndHold()
{
	// The weapon is free as soon as the hold is over, completed or not: a station bought with the key
	// still down does not keep the gun silent until the key comes up.
	ReleaseHands();

	AActor* Target = HeldTarget.Get();
	HeldTarget = nullptr;

	if (Target)
	{
		IRSInteractable::Execute_OnInteractionHoldEnded(Target, GetAvatarActorFromActorInfo());
	}
}
