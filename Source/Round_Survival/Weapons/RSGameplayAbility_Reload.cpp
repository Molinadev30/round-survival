// Round Survival

#include "Weapons/RSGameplayAbility_Reload.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/RSHandlingSet.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Equipment/RSWeaponInstance.h"
#include "Weapons/RSWeaponItemInstance.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

URSGameplayAbility_Reload::URSGameplayAbility_Reload(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;

	// Held for as long as the ability runs. Animation reads it, and firing cancels this ability
	// through it rather than by knowing what a reload is.
	ActivationOwnedTags.AddTag(RSGameplayTags::State_Reloading);

	// No reloading a weapon that is on its way in or out of the player's hands — mid-switch, or coming
	// back up after going down — nor while a revive or a station has them. Repairing, opening a gate
	// or topping up ammo leave the hands free, by design.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_SwitchingWeapon);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_GoingDown);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_HandsBusy);

	// Reloading with the trigger held stops the burst. Firing again takes a fresh pull of the
	// trigger, which cancels this in turn: the fire ability will not resume from a held trigger
	// while State.Reloading is on.
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Fire);

	// And it ends a sprint, by design: no reloading on the run.
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Sprint);

	// The asset tag is what the fire ability cancels by.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Reload);
	SetAssetTags(AssetTags);
}

URSWeaponInstance* URSGameplayAbility_Reload::GetWeaponInstance() const
{
	return Cast<URSWeaponInstance>(GetAssociatedEquipment());
}

int32 URSGameplayAbility_Reload::GetMagazineSize(const URSWeaponItemInstance* Item)
{
	// Capacity is a tag stack like the ammo itself, so a weapon that gets an extended magazine
	// changes one number in data and nothing else has to know.
	return Item ? Item->GetStatTagStackCount(RSGameplayTags::Weapon_MagazineSize) : 0;
}

bool URSGameplayAbility_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const URSWeaponItemInstance* Item = FindItemFromSpec(Handle, ActorInfo);
	if (!Item)
	{
		return false;
	}

	const int32 Magazine = Item->GetStatTagStackCount(RSGameplayTags::Weapon_Ammo_Magazine);
	const int32 Reserve = Item->GetStatTagStackCount(RSGameplayTags::Weapon_Ammo_Reserve);

	// A full magazine or an empty reserve both mean there is nothing to do. Blocking here rather
	// than starting and ending immediately keeps the animation from twitching on every press.
	if (Reserve <= 0 || Magazine >= GetMagazineSize(Item))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(RSGameplayTags::Ability_ActivateFail_NoAmmo);
		}

		return false;
	}

	return true;
}

float URSGameplayAbility_Reload::GetScaledReloadDuration() const
{
	float Multiplier = 1.0f;

	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		bool bFound = false;
		const float AttributeValue = ASC->GetGameplayAttributeValue(URSHandlingSet::GetReloadSpeedMultiplierAttribute(), bFound);

		if (bFound && AttributeValue > 0.0f)
		{
			Multiplier = AttributeValue;
		}
	}

	// The attribute is a speed, so it divides the duration: 2x speed is half the time.
	return ReloadDuration / Multiplier;
}

void URSGameplayAbility_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// The reload montage decides how long this takes when the weapon has one, so the rounds land
	// when the magazine goes in on screen rather than on a stopwatch that disagrees with it. The
	// play rate carries the handling attribute, so a faster reload is a faster animation too.
	float WaitDuration = GetScaledReloadDuration();
	float TransferFraction = 1.0f;

	if (const URSWeaponInstance* Weapon = GetWeaponInstance())
	{
		const float PlayRate = (ReloadDuration > 0.0f) ? (ReloadDuration / WaitDuration) : 1.0f;
		const float MontageDuration = Weapon->GetReloadDuration();

		if (MontageDuration > 0.0f)
		{
			WaitDuration = MontageDuration / PlayRate;
		}

		TransferFraction = Weapon->GetReloadAmmoTransferFraction();

		// Played by a cue so everybody else sees the reload; the weapon fills the spare magazine
		// from the item as it starts, before any rounds have moved. Within the activation's
		// prediction window, so the reloading player plays it now and the server's copy skips them.
		//
		// Added for the life of this ability rather than executed once: a reload is a state, not an
		// event. However the ability ends, the cue comes off with it on every machine, and a reload
		// cut short — by going down, by a revive or a station — stops its animation there. One that runs to
		// the end has finished its montage by then, so the removal stops nothing.
		FGameplayCueParameters CueParameters;
		CueParameters.RawMagnitude = PlayRate;
		K2_AddGameplayCueWithParams(RSGameplayTags::Cue_Weapon_Reload, CueParameters, /*bRemoveOnAbilityEnd=*/ true);
	}

	const float TransferDelay = WaitDuration * TransferFraction;
	DurationAfterTransfer = WaitDuration - TransferDelay;

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, TransferDelay);
	WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnAmmoTransferPoint);
	WaitTask->ReadyForActivation();
}

void URSGameplayAbility_Reload::OnAmmoTransferPoint()
{
	// Cancelled reloads never reach here: cancelling ends the task with the ability.
	TransferAmmo();

	if (DurationAfterTransfer <= KINDA_SMALL_NUMBER)
	{
		OnReloadFinished();
		return;
	}

	// The rounds are in, but the animation is not over: the ability keeps RS.State.Reloading until
	// it is, so nothing plays over the tail of the reload.
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, DurationAfterTransfer);
	WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnReloadFinished);
	WaitTask->ReadyForActivation();
}

void URSGameplayAbility_Reload::TransferAmmo()
{
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		// On the item, which is where the rounds live; the weapon instance only shows them.
		if (URSWeaponItemInstance* Item = GetAssociatedItem())
		{
			const int32 Magazine = Item->GetStatTagStackCount(RSGameplayTags::Weapon_Ammo_Magazine);
			const int32 Reserve = Item->GetStatTagStackCount(RSGameplayTags::Weapon_Ammo_Reserve);
			const int32 Space = FMath::Max(0, GetMagazineSize(Item) - Magazine);
			const int32 Transferred = FMath::Min(Space, Reserve);

			if (Transferred > 0)
			{
				// Removed first: if this ever ran twice, the second pass finds an empty reserve
				// instead of duplicating rounds into the magazine.
				Item->RemoveStatTagStack(RSGameplayTags::Weapon_Ammo_Reserve, Transferred);
				Item->AddStatTagStack(RSGameplayTags::Weapon_Ammo_Magazine, Transferred);

				UE_LOG(LogRSAbility, Verbose, TEXT("Reloaded [%s]: %d rounds, magazine %d, reserve %d."),
					*GetNameSafe(Item), Transferred, Magazine + Transferred, Reserve - Transferred);
			}
		}
	}
}

void URSGameplayAbility_Reload::OnReloadFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}
