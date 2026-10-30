// Round Survival

#pragma once

#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "RSPowerUpSettings.generated.h"

class ARSPowerUpPickup;
class URSPowerUpDefinition;

/** One power-up a kill can drop, and how likely it is against the others. */
USTRUCT(BlueprintType)
struct FRSPowerUpDrop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power-Ups")
	TObjectPtr<const URSPowerUpDefinition> PowerUp;

	/** Relative, not a percentage. Zero never drops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power-Ups", meta = (ClampMin = 0.0))
	float Weight = 1.0f;
};

/**
 * When kills drop power-ups, and which ones: every number of the rule, as data.
 *
 * A kill by a player has `DropChance` of dropping one, but never more than `MaxDropsPerRound` in a
 * round, and never two within `MinKillsBetweenDrops` player kills of each other, so a lucky streak
 * cannot carpet the floor. Which one is a weighted draw over `PowerUps`.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSPowerUpSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** A weighted draw over the drop table, or null when it is empty. */
	const URSPowerUpDefinition* PickPowerUp() const;

	/** The power-up whose asset name contains Name, for the debug command. */
	const URSPowerUpDefinition* FindPowerUp(const FString& Name) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Power-Ups", meta = (TitleProperty = "PowerUp"))
	TArray<FRSPowerUpDrop> PowerUps;

	/** What is spawned on the floor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Power-Ups")
	TSubclassOf<ARSPowerUpPickup> PickupClass;

	/** Chance that a player's kill drops one, when nothing else holds it back. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop Rule", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DropChance = 0.025f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop Rule", meta = (ClampMin = 0))
	int32 MaxDropsPerRound = 4;

	/** Player kills that have to happen after a drop before the next one can. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop Rule", meta = (ClampMin = 0))
	int32 MinKillsBetweenDrops = 15;

	/** Seconds a power-up lies on the floor before it goes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup", meta = (ForceUnits = s, ClampMin = 1.0))
	float PickupLifetime = 30.0f;

	/** The last seconds of that, during which it blinks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup", meta = (ForceUnits = s, ClampMin = 0.0))
	float PickupBlinkSeconds = 8.0f;

	/** How far from a dead enemy the floor may be found. Beyond it nothing drops: the body was off the map. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup", meta = (ForceUnits = cm, ClampMin = 0.0))
	float NavigationProjectionExtent = 200.0f;
};
