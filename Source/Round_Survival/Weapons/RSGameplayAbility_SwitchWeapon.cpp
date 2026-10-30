// Round Survival

#include "Weapons/RSGameplayAbility_SwitchWeapon.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Equipment/RSEquipmentDefinition.h"
#include "Equipment/RSEquipmentManagerComponent.h"
#include "Equipment/RSWeaponInstance.h"
#include "GameFramework/Actor.h"
#include "RSGameplayTags.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponItemInstance.h"
#include "Weapons/RSWeaponSlotsComponent.h"

URSGameplayAbility_SwitchWeapon::URSGameplayAbility_SwitchWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_SwitchWeapon);
	SetAssetTags(AssetTags);

	// Held for the whole holster-and-draw. Firing, reloading and aiming are blocked by it, and so is
	// a second switch: the four grants of this ability are separate specs, and without this a wheel
	// flick mid-switch would start another one on top.
	ActivationOwnedTags.AddTag(RSGameplayTags::State_SwitchingWeapon);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_SwitchingWeapon);

	// A downed player keeps the weapon they went down with: they can fire and reload it, not trade it.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Downed);

	// Nor is the weapon put away during a hold that takes both hands. The Weapon Forge upgrades the
	// weapon in hand when its hold completes, and that must be the weapon it was started on.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_HandsBusy);

	// Switching mid-reload abandons the reload, as firing does. It lowers the sights too: the holster
	// starts from the hip.
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Reload);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_ADS);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Sprint);
}

URSWeaponSlotsComponent* URSGameplayAbility_SwitchWeapon::FindWeaponSlots(const FGameplayAbilityActorInfo* ActorInfo)
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return Avatar ? Avatar->FindComponentByClass<URSWeaponSlotsComponent>() : nullptr;
}

int32 URSGameplayAbility_SwitchWeapon::ResolveTargetSlot(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo)
{
	const URSWeaponSlotsComponent* WeaponSlots = FindWeaponSlots(ActorInfo);
	if (!WeaponSlots || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return INDEX_NONE;
	}

	const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		return INDEX_NONE;
	}

	// The input tag this grant was given under says where it goes.
	const FGameplayTagContainer& GrantTags = Spec->GetDynamicSpecSourceTags();

	int32 Result = INDEX_NONE;

	if (GrantTags.HasTagExact(RSGameplayTags::InputTag_WeaponSlot_1))
	{
		Result = 0;
	}
	else if (GrantTags.HasTagExact(RSGameplayTags::InputTag_WeaponSlot_2))
	{
		Result = 1;
	}
	else if (GrantTags.HasTagExact(RSGameplayTags::InputTag_WeaponSlot_Next))
	{
		Result = WeaponSlots->FindAdjacentOccupiedSlot(+1);
	}
	else if (GrantTags.HasTagExact(RSGameplayTags::InputTag_WeaponSlot_Previous))
	{
		Result = WeaponSlots->FindAdjacentOccupiedSlot(-1);
	}

	// Nowhere to go: an empty slot, or the weapon already in hand.
	if (!WeaponSlots->GetItemInSlot(Result) || Result == WeaponSlots->GetActiveSlotIndex())
	{
		return INDEX_NONE;
	}

	return Result;
}

float URSGameplayAbility_SwitchWeapon::GetDrawDuration(const URSWeaponSlotsComponent* WeaponSlots, int32 SlotIndex)
{
	const URSWeaponItemInstance* Item = WeaponSlots ? WeaponSlots->GetItemInSlot(SlotIndex) : nullptr;
	const URSWeaponItemDefinition* ItemDefinition = Item ? Item->GetDefinition() : nullptr;
	if (!ItemDefinition || !ItemDefinition->EquipmentDefinition)
	{
		return 0.0f;
	}

	// Read off the class default object: the weapon instance that will play the draw does not exist
	// yet on this machine, and on a client it may not exist for a while.
	const URSEquipmentDefinition* EquipmentCDO = GetDefault<URSEquipmentDefinition>(ItemDefinition->EquipmentDefinition);
	const UClass* InstanceType = EquipmentCDO->InstanceType;
	const URSWeaponInstance* WeaponCDO = InstanceType ? Cast<URSWeaponInstance>(InstanceType->GetDefaultObject()) : nullptr;

	return WeaponCDO ? WeaponCDO->GetEquipDuration() : 0.0f;
}

bool URSGameplayAbility_SwitchWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Checked again on the server against its own slots, which is the check that counts: a client
	// asking for a slot the server does not think is full simply gets nothing.
	return ResolveTargetSlot(Handle, ActorInfo) != INDEX_NONE;
}

void URSGameplayAbility_SwitchWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	TargetSlot = ResolveTargetSlot(Handle, ActorInfo);

	// The drawn weapon puts itself away. No weapon in hand — the first draw after a respawn — is a
	// holster of zero length, and the draw follows at once.
	float HolsterDuration = 0.0f;

	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const URSEquipmentManagerComponent* EquipmentManager = Avatar ? Avatar->FindComponentByClass<URSEquipmentManagerComponent>() : nullptr;

	if (const URSWeaponInstance* DrawnWeapon = EquipmentManager ? EquipmentManager->GetFirstInstanceOfType<URSWeaponInstance>() : nullptr)
	{
		HolsterDuration = DrawnWeapon->GetHolsterDuration();

		// Played by a cue so everybody else sees the weapon go away before the new one comes up.
		K2_ExecuteGameplayCueWithParams(RSGameplayTags::Cue_Weapon_Holster, FGameplayCueParameters());
	}

	if (HolsterDuration <= KINDA_SMALL_NUMBER)
	{
		OnHolstered();
		return;
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, HolsterDuration);
	WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnHolstered);
	WaitTask->ReadyForActivation();
}

void URSGameplayAbility_SwitchWeapon::OnHolstered()
{
	URSWeaponSlotsComponent* WeaponSlots = FindWeaponSlots(CurrentActorInfo);

	// The swap is the server's alone. A predicting client keeps waiting and receives the new weapon
	// by replication, which draws itself when it arrives.
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && WeaponSlots)
	{
		WeaponSlots->SetActiveSlot(TargetSlot);
	}

	const float DrawDuration = GetDrawDuration(WeaponSlots, TargetSlot);
	if (DrawDuration <= KINDA_SMALL_NUMBER)
	{
		OnDrawn();
		return;
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, DrawDuration);
	WaitTask->OnFinish.AddDynamic(this, &ThisClass::OnDrawn);
	WaitTask->ReadyForActivation();
}

void URSGameplayAbility_SwitchWeapon::OnDrawn()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}
