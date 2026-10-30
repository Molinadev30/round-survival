// Round Survival

#pragma once

#include "AbilitySystem/Costs/RSAbilityCost.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "RSAbilityCost_Scrap.generated.h"

class UGameplayEffect;

/**
 * Paying for something with scrap, at the price the target names.
 *
 * The cost object holds no number. GAS models a cost as data on the ability, which works for
 * ammo — one weapon, one price per shot — and does not work here: interaction is a single ability
 * on a single key, and the locker, the gate and the station all cost different amounts. So the
 * price is read from whatever is being interacted with, through `IRSInteractable::GetScrapCost`,
 * every time it is checked or charged.
 *
 * It is still an ability cost rather than a subtraction inside the purchasable, and that is the
 * point: activation blocks itself when the player cannot pay, the refusal carries
 * `RS.Ability.ActivateFail.NoScrap` for the HUD to explain, and a phase-2 discount station has one
 * place to modify. Spending goes to `Scrap` directly and never through `ScrapGain`, so an income
 * buff cannot make things cheaper.
 */
UCLASS()
class ROUND_SURVIVAL_API URSAbilityCost_Scrap : public URSAbilityCost
{
	GENERATED_BODY()

public:
	URSAbilityCost_Scrap();

	//~URSAbilityCost interface
	virtual bool CheckCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ApplyCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End of URSAbilityCost interface

	/** What this ability would pay right now, given what its owner is looking at. Zero when free. */
	static int32 GetPendingCost(const URSGameplayAbility* Ability, const FGameplayAbilityActorInfo* ActorInfo);

protected:
	/**
	 * The effect that moves the scrap.
	 *
	 * Instant, additive on `Scrap`, magnitude from `RS.SetByCaller.Scrap`. Charging through an
	 * effect rather than writing the attribute keeps purchases on the same path as awards, which
	 * is what lets a later modifier touch one and not the other.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Costs)
	TSubclassOf<UGameplayEffect> CostEffect;

	/** Added to the failure tags when the player cannot afford it, so the HUD can say why. */
	UPROPERTY(EditDefaultsOnly, Category = Costs, meta = (Categories = "RS.Ability.ActivateFail"))
	FGameplayTag FailureTag;
};
