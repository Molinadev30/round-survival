// Round Survival

#pragma once

#include "UObject/Object.h"

// Passed by value below, so a forward declaration is not enough: the compiler needs the full
// type to know how to copy them.
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayAbilitySpecHandle.h"

#include "RSAbilityCost.generated.h"

class URSGameplayAbility;
struct FGameplayTagContainer;

/**
 * Base class for a cost an ability has to pay.
 *
 * GAS only models costs as gameplay effects on attributes, which does not fit ammo: a magazine
 * belongs to a weapon, not to a pawn, and a player carrying two weapons would need two attribute
 * sets for it. These cost objects are instanced on the ability instead, so an ability can spend
 * a tag stack on its weapon, scrap on the player, or both, and still block itself automatically
 * when it cannot pay.
 */
UCLASS(DefaultToInstanced, EditInlineNew, Abstract)
class ROUND_SURVIVAL_API URSAbilityCost : public UObject
{
	GENERATED_BODY()

public:
	URSAbilityCost()
	{
	}

	/**
	 * Can this cost be paid right now?
	 *
	 * Called from CanActivateAbility, so returning false stops the ability from starting at all
	 * rather than letting it run and fail halfway.
	 */
	virtual bool CheckCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
	{
		return true;
	}

	/** Spend it. Only called after every cost on the ability passed CheckCost. */
	virtual void ApplyCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
	{
	}

	/**
	 * If true, the ability defers this cost until it confirms a hit.
	 *
	 * Reserved for costs that should only be charged on success. Ammo is not one of them: a
	 * missed shot still leaves the chamber empty.
	 */
	bool ShouldOnlyApplyCostOnHit() const
	{
		return bOnlyApplyCostOnHit;
	}

protected:
	UPROPERTY(EditDefaultsOnly, Category = Costs)
	bool bOnlyApplyCostOnHit = false;
};
