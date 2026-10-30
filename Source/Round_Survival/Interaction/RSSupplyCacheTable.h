// Round Survival

#pragma once

#include "Engine/DataAsset.h"

#include "RSSupplyCacheTable.generated.h"

class URSWeaponItemDefinition;
class URSWeaponSlotsComponent;

/** One weapon the Supply Cache can hand out, and how likely it is against the others. */
USTRUCT(BlueprintType)
struct FRSSupplyCacheEntry
{
	GENERATED_BODY()

	/** What the player walks away with: an item, as everything that gives a weapon hands over. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply Cache")
	TObjectPtr<const URSWeaponItemDefinition> Item;

	/** Relative, not a percentage: an entry of 2 comes up twice as often as an entry of 1. Zero never does. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply Cache", meta = (ClampMin = 0.0))
	float Weight = 1.0f;
};

/**
 * What a Supply Cache can hand out, as data.
 *
 * The draw is weighted and leaves out what the player already carries: a weapon they hold would
 * be refused by their slots, and paying for one would be paying for nothing. It is only ever
 * drawn on the server, at the moment the purchase is paid for.
 *
 * Several caches can share one table, and a new weapon in the cache is one more entry.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSSupplyCacheTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** A weighted draw among the entries this carrier can take, or null when there are none. */
	const URSWeaponItemDefinition* Roll(const URSWeaponSlotsComponent* Carrier) const;

	/** Whether a draw for this carrier would find anything. What the cache asks before it charges. */
	bool HasAnythingFor(const URSWeaponSlotsComponent* Carrier) const;

	/**
	 * Any entry but Previous, ignoring weights and the carrier: what the cache shows while it
	 * rolls, which is decoration and decides nothing.
	 */
	const URSWeaponItemDefinition* PickForDisplay(const URSWeaponItemDefinition* Previous) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Cache", meta = (TitleProperty = "Item"))
	TArray<FRSSupplyCacheEntry> Entries;

private:
	bool IsEligible(const FRSSupplyCacheEntry& Entry, const URSWeaponSlotsComponent* Carrier) const;
};
