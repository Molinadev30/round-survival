// Round Survival

#pragma once

#include "AbilitySystem/Costs/RSAbilityCost.h"
#include "GameplayTagContainer.h"

#include "RSAbilityCost_ItemTagStack.generated.h"

/**
 * Spends a tag stack on the weapon item the ability's equipment was drawn from. This is how ammo is
 * charged.
 *
 * The equipment is the ability spec's source object, and the item is that equipment's instigator.
 * Nothing holds a pointer to either, and an ability granted by something other than a drawn weapon
 * simply fails the check.
 */
UCLASS(meta = (DisplayName = "Spend Item Tag Stack"))
class ROUND_SURVIVAL_API URSAbilityCost_ItemTagStack : public URSAbilityCost
{
	GENERATED_BODY()

public:
	URSAbilityCost_ItemTagStack();

	//~URSAbilityCost interface
	virtual bool CheckCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ApplyCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End of URSAbilityCost interface

protected:
	/** Which stack to spend. `RS.Weapon.Ammo.Magazine` for firing. */
	UPROPERTY(EditDefaultsOnly, Category = "Costs", meta = (Categories = "RS.Weapon.Ammo"))
	FGameplayTag Tag;

	UPROPERTY(EditDefaultsOnly, Category = "Costs", meta = (ClampMin = 1))
	int32 Quantity = 1;

	/** Reported back to the caller when the stack is too low, so the HUD can say why. */
	UPROPERTY(EditDefaultsOnly, Category = "Costs")
	FGameplayTag FailureTag;
};
