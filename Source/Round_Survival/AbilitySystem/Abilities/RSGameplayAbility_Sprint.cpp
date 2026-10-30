// Round Survival

#include "AbilitySystem/Abilities/RSGameplayAbility_Sprint.h"

#include "Core/RSCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "RSGameplayTags.h"

URSGameplayAbility_Sprint::URSGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Held, and tried again every frame the key stays down while it is not running: a sprint cut short
	// by a shot or the sights comes back by itself once they are over.
	ActivationPolicy = ERSAbilityActivationPolicy::WhileInputActive;

	ActivationOwnedTags.AddTag(RSGameplayTags::State_Sprinting);

	// Whatever has the hands, and for as long as it has them. Each of these also cancels a sprint
	// already running when it starts; these stop the held key from starting it again underneath.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Firing);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_ADS);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Reloading);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_SwitchingWeapon);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_HandsBusy);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Downed);

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Sprint);
	SetAssetTags(AssetTags);
}

void URSGameplayAbility_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	SetMovementSprintFlag(ActorInfo, true);

	// Sprinting stands a crouching player up. Crouching is predicted by the engine the same way, so
	// this is the controlling machine's call too. Not a sliding one: a slide is crouched, and the key
	// coming back mid-slide — after a shot, say — is what stands the player up into a sprint at its end.
	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const URSCharacterMovementComponent* Movement = Character ? Cast<URSCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (Character && ActorInfo->IsLocallyControlled() && Character->bIsCrouched && !(Movement && Movement->IsSliding()))
	{
		Character->UnCrouch();
	}
}

void URSGameplayAbility_Sprint::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
	}
}

void URSGameplayAbility_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// However it ends: released, or cut by something that needs the hands.
	SetMovementSprintFlag(ActorInfo, false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_Sprint::SetMovementSprintFlag(const FGameplayAbilityActorInfo* ActorInfo, bool bSprinting)
{
	// The server takes the flag from the client's moves, as it does the aim flag.
	if (!ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (URSCharacterMovementComponent* Movement = Character ? Cast<URSCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		Movement->SetWantsToSprint(bSprinting);
	}
}
