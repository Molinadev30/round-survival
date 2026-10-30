// Round Survival

#include "Weapons/RSGameplayAbility_AimDownSights.h"

#include "Core/RSCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "RSGameplayTags.h"

URSGameplayAbility_AimDownSights::URSGameplayAbility_AimDownSights(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Held, and tried again every frame the button stays down while it is not running: a player who
	// keeps the button down through going down is aiming again as soon as the weapon is back up. Not
	// through a weapon switch: each weapon grants its own copy of this ability, and the held input
	// belongs to the copy that was there when the button went down.
	ActivationPolicy = ERSAbilityActivationPolicy::WhileInputActive;

	// The state everything else reads. Owned by the activation, so it is on the aiming player's
	// machine the moment it starts and on the server as soon as the activation arrives.
	ActivationOwnedTags.AddTag(RSGameplayTags::State_ADS);

	// Not with a weapon on its way in or out of the hands, nor while a revive or a station has them.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_SwitchingWeapon);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_GoingDown);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_HandsBusy);

	// Aiming wins over sprinting: it ends a sprint, and the sprint stays blocked while the sights are up.
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Sprint);

	// What switching, going down and those holds cancel by.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_ADS);
	SetAssetTags(AssetTags);
}

void URSGameplayAbility_AimDownSights::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	SetMovementAimFlag(ActorInfo, true);

	// Heard by everybody: executed from the predicting client it plays there at once, and the
	// server passes it on to the others.
	K2_ExecuteGameplayCueWithParams(RSGameplayTags::Cue_Weapon_AimIn, FGameplayCueParameters());
}

void URSGameplayAbility_AimDownSights::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	// The aiming player's release ends it, and the end goes to the server with it.
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
	}
}

void URSGameplayAbility_AimDownSights::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// However it ends — released, cancelled by a switch or by going down, or revoked with the weapon.
	SetMovementAimFlag(ActorInfo, false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_AimDownSights::SetMovementAimFlag(const FGameplayAbilityActorInfo* ActorInfo, bool bAiming)
{
	// The server learns the flag from the client's moves, never from its own copy of this ability: set
	// here as well, it would be on for moves the client made before it started aiming.
	if (!ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (URSCharacterMovementComponent* Movement = Character ? Cast<URSCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		Movement->SetWantsToAimDownSights(bAiming);
	}
}
