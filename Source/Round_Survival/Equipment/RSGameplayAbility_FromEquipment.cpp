// Round Survival

#include "Equipment/RSGameplayAbility_FromEquipment.h"

#include "AbilitySystemComponent.h"
#include "Equipment/RSEquipmentInstance.h"
#include "Weapons/RSWeaponItemInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RSGameplayAbility_FromEquipment"

URSGameplayAbility_FromEquipment::URSGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

URSEquipmentInstance* URSGameplayAbility_FromEquipment::GetAssociatedEquipment() const
{
	// Read from the spec rather than from CurrentSourceObject, so this also answers correctly
	// before activation — CheckCost and CanActivateAbility both run at that point.
	if (const FGameplayAbilitySpec* Spec = UGameplayAbility::GetCurrentAbilitySpec())
	{
		return Cast<URSEquipmentInstance>(Spec->SourceObject.Get());
	}

	return nullptr;
}

URSEquipmentInstance* URSGameplayAbility_FromEquipment::FindEquipmentFromSpec(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return nullptr;
	}

	if (const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
	{
		return Cast<URSEquipmentInstance>(Spec->SourceObject.Get());
	}

	return nullptr;
}

URSWeaponItemInstance* URSGameplayAbility_FromEquipment::GetAssociatedItem() const
{
	const URSEquipmentInstance* Equipment = GetAssociatedEquipment();
	return Equipment ? Cast<URSWeaponItemInstance>(Equipment->GetInstigator()) : nullptr;
}

URSWeaponItemInstance* URSGameplayAbility_FromEquipment::FindItemFromSpec(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo)
{
	const URSEquipmentInstance* Equipment = FindEquipmentFromSpec(Handle, ActorInfo);
	return Equipment ? Cast<URSWeaponItemInstance>(Equipment->GetInstigator()) : nullptr;
}

#undef LOCTEXT_NAMESPACE
