// Round Survival

#include "AbilitySystem/Costs/RSAbilityCost_ItemTagStack.h"

#include "Equipment/RSGameplayAbility_FromEquipment.h"
#include "RSGameplayTags.h"
#include "Weapons/RSWeaponItemInstance.h"

URSAbilityCost_ItemTagStack::URSAbilityCost_ItemTagStack()
{
	Tag = RSGameplayTags::Weapon_Ammo_Magazine;
	FailureTag = RSGameplayTags::Ability_ActivateFail_NoAmmo;
}

bool URSAbilityCost_ItemTagStack::CheckCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// Read through the spec rather than the ability's current source object: CheckCost runs before
	// activation, when there is no current source object yet.
	const URSWeaponItemInstance* Item = URSGameplayAbility_FromEquipment::FindItemFromSpec(Handle, ActorInfo);
	if (!Item)
	{
		// Not granted by a weapon drawn from an item, so there is no stack to spend. Failing closed
		// is deliberate: the alternative is an ability that fires for free because it was
		// misconfigured.
		return false;
	}

	const bool bCanPay = Item->GetStatTagStackCount(Tag) >= Quantity;

	if (!bCanPay && OptionalRelevantTags && FailureTag.IsValid())
	{
		OptionalRelevantTags->AddTag(FailureTag);
	}

	return bCanPay;
}

void URSAbilityCost_ItemTagStack::ApplyCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Only the server spends it. The client predicts the shot; the magazine count it displays
	// comes back down by replication, so a client that charged itself would double-count.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	if (URSWeaponItemInstance* Item = URSGameplayAbility_FromEquipment::FindItemFromSpec(Handle, ActorInfo))
	{
		Item->RemoveStatTagStack(Tag, Quantity);
	}
}
