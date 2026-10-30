// Round Survival

#pragma once

#include "Curves/CurveFloat.h"
#include "Enemies/RSEnemyTypes.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "RSRoundSettings.generated.h"

class ARSZombieCharacter;
class UGameplayEffect;
class URSSpawnSelector;

/**
 * How much of each round's horde moves at each speed.
 *
 * One weight curve per tier, read at the round number and drawn per enemy, so a round is a
 * mixture rather than a single speed: round six is mostly walkers with a few runners in it, and
 * the runners are what the player notices. A single tier per round would make the change a step
 * the player meets all at once, and every enemy in a round would behave identically.
 *
 * The weights are relative and do not have to add up to anything. A tier at zero cannot be drawn.
 */
USTRUCT(BlueprintType)
struct FRSMoveTierMix
{
	GENERATED_BODY()

	FRSMoveTierMix();

	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds")
	FRuntimeFloatCurve ShambleWeight;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds")
	FRuntimeFloatCurve WalkWeight;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds")
	FRuntimeFloatCurve RunWeight;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds")
	FRuntimeFloatCurve SprintWeight;

	/** Weighted draw for one enemy of this round. Falls back to Shamble if every weight is zero. */
	ERSMoveTier DrawTier(int32 RoundNumber) const;

	/** The four weights at a round, in tier order. For the debug read-out and the tests. */
	void GetWeights(int32 RoundNumber, float& OutShamble, float& OutWalk, float& OutRun, float& OutSprint) const;
};

/**
 * The difficulty curve, as data.
 *
 * Every number the round director uses comes from here and none of them is written in C++: how
 * many enemies a round releases, how much health they have, how fast they arrive, how many can be
 * alive at once and how long the breathing room between rounds lasts. Tuning the game is editing
 * this asset, and the director is the same code at round 1 and at round 40.
 *
 * The curves carry sensible keys from the constructor, so a freshly created asset already plays.
 */
UCLASS(BlueprintType)
class ROUND_SURVIVAL_API URSRoundSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URSRoundSettings();

	/** Enemies to release over the whole round, before the extra-player share. */
	UFUNCTION(BlueprintPure, Category = "RS|Rounds")
	int32 GetEnemyQuota(int32 RoundNumber, int32 PlayerCount) const;

	/** Maximum and starting health for an enemy of this round. */
	UFUNCTION(BlueprintPure, Category = "RS|Rounds")
	float GetEnemyHealth(int32 RoundNumber) const;

	/** Seconds between two releases while the round is spawning. */
	UFUNCTION(BlueprintPure, Category = "RS|Rounds")
	float GetSpawnInterval(int32 RoundNumber) const;

	/** Breathing room before the next round starts releasing. */
	UFUNCTION(BlueprintPure, Category = "RS|Rounds")
	float GetIntermissionSeconds(int32 RoundNumber) const;

	/** Ceiling on enemies alive at once. The performance budget, expressed as a rule. */
	UFUNCTION(BlueprintPure, Category = "RS|Rounds")
	int32 GetMaxAlive(int32 PlayerCount) const;

	/** Draws one enemy's speed tier out of this round's mixture. */
	UFUNCTION(BlueprintPure, Category = "RS|Rounds")
	ERSMoveTier DrawMoveTier(int32 RoundNumber) const;

	UFUNCTION(BlueprintPure, Category = "RS|Rounds")
	TSubclassOf<ARSZombieCharacter> GetEnemyClass() const { return EnemyClass; }

	UFUNCTION(BlueprintPure, Category = "RS|Rounds")
	TSubclassOf<UGameplayEffect> GetScalingEffect() const { return ScalingEffect; }

	const URSSpawnSelector* GetSpawnSelector() const { return SpawnSelector; }

	const FRSMoveTierMix& GetMoveTierMix() const { return MoveTierMix; }

	int32 GetPoolPrewarmCount() const { return PoolPrewarmCount; }
	int32 GetPoolMaxSize() const { return PoolMaxSize; }

protected:
	/** What the round releases. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds")
	TSubclassOf<ARSZombieCharacter> EnemyClass;

	/**
	 * Where they come from. Configured inline, so the policy and its numbers live in one asset.
	 *
	 * Left empty, the spawn subsystem uses a default weighted selector.
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "RS|Rounds")
	TObjectPtr<URSSpawnSelector> SpawnSelector;

	/**
	 * Instant effect that writes the round's health onto a newly spawned enemy.
	 *
	 * It has to override **both** `MaxHealth` and `Health` from the same SetByCaller magnitude.
	 * Raising the maximum alone would leave the enemy at the health its ability set gave it —
	 * born wounded — and `URSHealthSet` deliberately only clamps health downwards.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds")
	TSubclassOf<UGameplayEffect> ScalingEffect;

	/** Enemies per round for a single player, by round number. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Curves")
	FRuntimeFloatCurve EnemyQuotaCurve;

	/** Extra share of the quota per player beyond the first. 0.4 means +40 % each. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Curves", meta = (ClampMin = 0.0))
	float QuotaPerExtraPlayer = 0.4f;

	/** Enemy health by round number, before compounding. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Curves")
	FRuntimeFloatCurve EnemyHealthCurve;

	/**
	 * Round the health curve hands over to compounding.
	 *
	 * A hand-authored curve cannot be drawn out to round forty, and a flat extrapolation would
	 * make the late rounds trivial. Past this round, health is the curve's value here multiplied
	 * by the factor below once per round, which is how the genre's late game stays a wall.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Curves", meta = (ClampMin = 1))
	int32 CompoundingStartRound = 10;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Curves", meta = (ClampMin = 1.0))
	float HealthCompoundingFactor = 1.1f;

	/** Seconds between releases, by round number. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Curves")
	FRuntimeFloatCurve SpawnIntervalCurve;

	/** Seconds of intermission before a round starts, by round number. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Curves")
	FRuntimeFloatCurve IntermissionCurve;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Curves")
	FRSMoveTierMix MoveTierMix;

	/** Enemies alive at once for one player. The project's stated constraint, as a number. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds", meta = (ClampMin = 1))
	int32 MaxAliveBase = 64;

	/** Added to the ceiling for each player beyond the first. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds", meta = (ClampMin = 0))
	int32 MaxAlivePerExtraPlayer = 6;

	/**
	 * Enemies made and pooled as the match starts, so the early rounds hand out bodies instead of
	 * spawning them mid-fight. Paid once, while the players are arriving.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Pool", meta = (ClampMin = 0))
	int32 PoolPrewarmCount = 24;

	/**
	 * Most bodies the pool keeps waiting. A body that leaves with the pool full is destroyed, so the
	 * peak of one bad round is not kept alive for the rest of the match. Above the alive ceiling,
	 * because the corpses of a full horde are on their way back while it is still being refilled.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Rounds|Pool", meta = (ClampMin = 0))
	int32 PoolMaxSize = 80;
};
