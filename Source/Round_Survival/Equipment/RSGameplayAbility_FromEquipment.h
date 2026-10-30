// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_FromEquipment.generated.h"

class URSEquipmentInstance;
class URSWeaponItemInstance;

/**
 * An ability that belongs to a piece of equipment rather than to the pawn.
 *
 * The equipment is found through the ability spec's source object, which the ability set filled in
 * when the equipment was equipped. Nothing stores a pointer back to the weapon, so an ability can
 * never outlive or mismatch the thing that granted it: revoke the set and the ability goes too.
 *
 * The item is one step further: the equipment's instigator. That is where a weapon's ammo lives,
 * because the equipment instance does not survive a holster and the item does.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_FromEquipment : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "RS|Ability")
	URSEquipmentInstance* GetAssociatedEquipment() const;

	/**
	 * The same lookup for code that runs before activation.
	 *
	 * CanActivateAbility and CheckCost are called on an instance whose CurrentSpecHandle is not
	 * set yet, so they have to go through the handle they were passed instead.
	 */
	static URSEquipmentInstance* FindEquipmentFromSpec(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo);

	/** The weapon item the equipment was drawn from, or null for equipment that is not a weapon. */
	UFUNCTION(BlueprintCallable, Category = "RS|Ability")
	URSWeaponItemInstance* GetAssociatedItem() const;

	/** The same, before activation. See FindEquipmentFromSpec. */
	static URSWeaponItemInstance* FindItemFromSpec(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo);
};
