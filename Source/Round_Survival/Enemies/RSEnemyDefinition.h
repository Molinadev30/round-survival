// Round Survival

#pragma once

#include "Enemies/RSEnemyTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "RSEnemyDefinition.generated.h"

class UGameplayEffect;
class URSAbilitySet;
class URSDismembermentProfile;

/**
 * What an enemy type is, as data: its kit, how fast each tier moves, what it pays out, how it comes
 * apart, how long the body stays.
 *
 * The pawn Blueprint keeps what is wiring rather than tuning — mesh, animation Blueprint, capsule —
 * and points at one of these for everything a designer balances. A second enemy type is a second
 * asset, not a second set of numbers spread across a Blueprint's class defaults.
 *
 * Per-round scaling is not here. `URSRoundSettings` owns how a round changes an enemy; this owns
 * what the enemy is before any round touches it.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URSEnemyDefinition();

	/** Base speed for a tier, or null when the table has no entry for it. */
	const float* FindTierSpeed(ERSMoveTier Tier) const { return TierSpeeds.Find(Tier); }

	/** Scrap for a reward tag, or zero when this enemy pays nothing for it. */
	float GetScrapReward(const FGameplayTag& RewardTag) const;

	/**
	 * The enemy kit: its starting attribute values, its melee attack, its passives.
	 *
	 * Granted on possession, because a gameplay effect needs a fully initialised component to apply
	 * to. Round scaling is applied on top of this at spawn.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TObjectPtr<const URSAbilitySet>> AbilitySets;

	/**
	 * Base walking speed per tier, in centimetres per second.
	 *
	 * The movement component and the animation both read this one list: the pawn writes the entry
	 * for its tier into `MaxWalkSpeed`, and the animation switches loops at the midpoints between
	 * entries. The defaults are not free numbers — each is the speed its locomotion loop actually
	 * travels at, measured from the animation's root motion, times the play rate its blend space
	 * sample runs at. Raise one far above that and the feet start sliding, because nothing here
	 * warps a stride.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ForceUnits = "cm/s"))
	TMap<ERSMoveTier, float> TierSpeeds;

	/**
	 * How fast an enemy with a leg gone drags itself along, whatever its tier was: a runner that
	 * loses a leg crawls like a walker that does. Slows still apply on top.
	 *
	 * Not the speed of the raw crawl loop, which covers 72 cm in 9 s (8 cm/s) and would take two
	 * minutes over ten metres. The animation plays the loop faster to match this instead.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = 0.0, ForceUnits = "cm/s"))
	float CrawlSpeed = 25.0f;

	/**
	 * Scrap paid out per reward tag: `RS.Economy.Reward.Hit`, `.Kill`, `.Headshot`, `.Hit.Melee`,
	 * `.Kill.Melee`.
	 *
	 * A map rather than one float per case, so the melee rewards are data and a melee
	 * weapon only has to tag its damage effect.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Rewards", meta = (Categories = "RS.Economy.Reward"))
	TMap<FGameplayTag, float> ScrapRewards;

	/**
	 * The effect that pays out. Expected to write its `RS.SetByCaller.Scrap` magnitude into
	 * ScrapGain, so the economy set applies the income multiplier on its way into Scrap.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Rewards")
	TSubclassOf<UGameplayEffect> ScrapAwardEffect;

	/** How this enemy comes apart. None means it never loses a part. */
	UPROPERTY(EditDefaultsOnly, Category = "Dismemberment")
	TObjectPtr<const URSDismembermentProfile> Dismemberment;

	/** Seconds the ragdoll stays before the corpse leaves the world. */
	UPROPERTY(EditDefaultsOnly, Category = "Death", meta = (ForceUnits = s, ClampMin = 0.0))
	float CorpseLifetime = 5.0f;
};
