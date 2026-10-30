// Round Survival

#pragma once

#include "AbilitySystem/RSGameplayTagStack.h"
#include "UObject/Object.h"

#include "RSWeaponItemInstance.generated.h"

class APawn;
class URSWeaponItemDefinition;
class URSWeaponSkin;

DECLARE_MULTICAST_DELEGATE_OneParam(FRSWeaponItemStatsChanged, class URSWeaponItemInstance* /*Item*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FRSWeaponItemSkinChanged, class URSWeaponItemInstance* /*Item*/);

/**
 * One weapon a player owns.
 *
 * Outlives every draw and holster, which is the whole reason it exists: the equipment instance is
 * created when the weapon comes out and destroyed when it goes away, and the magazine has to come
 * back with it. So this is where the ammo lives, as tag stacks, spent by the fire ability's cost
 * and moved by the reload — both on the server, replicated down for the HUD and the magazine mesh.
 *
 * A replicated UObject outered to the pawn, replicated as a subobject of the weapon slots.
 */
UCLASS(BlueprintType)
class ROUND_SURVIVAL_API URSWeaponItemInstance : public UObject
{
	GENERATED_BODY()

public:
	//~UObject interface
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual UWorld* GetWorld() const override final;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of UObject interface

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	const URSWeaponItemDefinition* GetDefinition() const { return Definition; }

	/** Server only, once, by the weapon slots when the item is created. */
	void SetDefinition(const URSWeaponItemDefinition* InDefinition) { Definition = InDefinition; }

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	APawn* GetPawn() const;

	// -- Stat tag stacks ---------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Weapon")
	void AddStatTagStack(FGameplayTag Tag, int32 StackCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Weapon")
	void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount);

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	int32 GetStatTagStackCount(FGameplayTag Tag) const;

	/** Puts every stack back to the definition's starting value: what a new copy starts with. Server only. */
	void ResetStatsToInitial();

	/**
	 * Tops the reserve up to full — the definition's starting value, or the upgrade's — and leaves the
	 * magazine as it is. Server only.
	 *
	 * What a Wall Locker refill buys. The magazine is not touched, so a player who buys ammo with
	 * an empty magazine still has to reload — and the rounds missing from that magazine are not
	 * added to the reserve.
	 */
	void RefillReserveAmmo();

	/**
	 * Fills the magazine to its size and the reserve to full, the upgraded sizes for an upgraded
	 * copy. What the Resupply power-up hands every weapon. Server only.
	 */
	void RefillAllAmmo();

	/** Whether the reserve is full: "nothing to refill", for a Wall Locker that must refuse before it charges. */
	bool IsReserveAmmoFull() const;

	/**
	 * Any stack or the upgrade level changed, on any machine: on the server as it happens, from the
	 * rep notifies on a client. Says nothing about which — listeners reread what they care about.
	 */
	FRSWeaponItemStatsChanged OnStatsChanged;

	// -- Weapon Forge ------------------------------------------------------------------------------

	/** Zero for the weapon as it comes, one once the Weapon Forge has upgraded this copy. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	int32 GetUpgradeLevel() const { return UpgradeLevel; }

	/** Whether the Weapon Forge would take this copy: an upgradable weapon, not upgraded yet. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	bool CanBeUpgraded() const;

	/**
	 * Upgrades this copy and no other, by its definition's `Upgrade`: the level, a bigger magazine
	 * and reserve, both filled, and the upgraded finish. Server only. False, and nothing changed, if
	 * `CanBeUpgraded` says no.
	 */
	bool ApplyUpgrade();

	/** What the HUD and the prompts call this copy: the upgraded name once it is upgraded. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	FText GetDisplayName() const;

	/** What the weapon's base damage is multiplied by: the upgrade's, or one. */
	float GetDamageMultiplier() const;

	/** What a Wall Locker charges to refill this copy: the upgrade's price once it is upgraded. */
	int32 GetRefillCost() const;

	// -- Skin --------------------------------------------------------------------------------------

	/** The finish this copy shows: its override if it has one, otherwise its definition's. Null is the mesh's own. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	const URSWeaponSkin* GetSkin() const;

	/**
	 * Gives this one copy a different finish; null goes back to the definition's. Server only.
	 *
	 * The Weapon Forge's way in: upgrading a weapon changes this copy and no other, which is why
	 * the choice lives here and replicates rather than on the shared definition.
	 */
	void SetSkinOverride(const URSWeaponSkin* InSkin);

	/** The skin changed, on any machine: from SetSkinOverride on the server, from the rep notify on a client. */
	FRSWeaponItemSkinChanged OnSkinChanged;

private:
	UFUNCTION()
	void OnRep_StatTags();

	UFUNCTION()
	void OnRep_SkinOverride();

	UFUNCTION()
	void OnRep_UpgradeLevel();

	/** A full reserve: the definition's starting value, or the upgrade's once upgraded. */
	int32 GetFullReserveAmmo() const;

	/** Adds or removes the difference, so the stack ends at Count in one replicated change. Server only. */
	void SetStatTagStack(FGameplayTag Tag, int32 Count);

	/** Only ever 0 or 1 today. A level rather than a flag, so the number is already where a second one would go. */
	UPROPERTY(ReplicatedUsing = OnRep_UpgradeLevel)
	int32 UpgradeLevel = 0;

	UPROPERTY(Replicated)
	TObjectPtr<const URSWeaponItemDefinition> Definition;

	UPROPERTY(ReplicatedUsing = OnRep_StatTags)
	FRSGameplayTagStackContainer StatTags;

	UPROPERTY(ReplicatedUsing = OnRep_SkinOverride)
	TObjectPtr<const URSWeaponSkin> SkinOverride;
};
