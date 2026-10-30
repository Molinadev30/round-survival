// Round Survival

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "RSStationDefinition.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * What a buff station gives, as data: its name, its price, the effect it applies and the tag that
 * says a player has it.
 *
 * Vitality, Rapid Hands, Overcharge and Second Wind are four of these and one station class. A fifth
 * station is a fifth asset: no enum to extend, no C++ to compile.
 *
 * **The tag lives here, not on the effect.** `GrantTo` puts it on the effect spec as a dynamic
 * granted tag, so the player carries it exactly as long as the effect and loses both together, and
 * an effect asset cannot claim to be one station while this asset names another. It is also why
 * anything that hands out a station — the station itself, a future power-up — goes through
 * `GrantTo`: applying the effect any other way gives the buff without the tag that says it is there.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSStationDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Applies the effect to this ability system, carrying the station's tag. Server only.
	 *
	 * Refused, with an invalid handle, when the target already has the station: a second copy would
	 * stack the buff, and the station's own check has already said no by the time a purchase gets here.
	 */
	FActiveGameplayEffectHandle GrantTo(UAbilitySystemComponent* ASC) const;

	/** Whether this ability system has the station. Reads the replicated tag, so it answers on the owning client too. */
	bool IsGrantedTo(const UAbilitySystemComponent* ASC) const;

	/** False when the asset is missing its effect or its tag, and so cannot grant anything. */
	bool IsValidStation() const;

	/** What the prompt calls the station. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station")
	FText DisplayName;

	/** Scrap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station", meta = (ClampMin = 0))
	int32 Cost = 2500;

	/**
	 * The buff. Infinite, carrying no tag of its own, and removed by its own tag requirements when its
	 * owner goes down — so losing every station on a down is data on the effect, not code anywhere.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station")
	TSubclassOf<UGameplayEffect> Effect;

	/** What a player who has this station carries. Abilities, the HUD and the match ask for it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station", meta = (Categories = "RS.Station"))
	FGameplayTag StationTag;
};
