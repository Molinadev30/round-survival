// Round Survival

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "RSWeaponItemDefinition.generated.h"

class UTexture2D;
class URSEquipmentDefinition;
class URSWeaponAssembly;
class URSWeaponSkin;

/**
 * What the Weapon Forge makes of a weapon, as data on the weapon: each weapon is upgraded by its
 * own numbers, and one level is all there is.
 */
USTRUCT(BlueprintType)
struct FRSWeaponUpgrade
{
	GENERATED_BODY()

	/** What the HUD and the prompts call the upgraded weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Forge")
	FText DisplayName;

	/**
	 * Multiplies the weapon's base damage. On the weapon, not the player's outgoing damage
	 * multiplier, which would raise the other weapon they carry as well.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Forge", meta = (ClampMin = 0.0))
	float DamageMultiplier = 2.0f;

	/** Magazine capacity once upgraded. The magazine is filled to it by the upgrade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Forge", meta = (ClampMin = 1))
	int32 MagazineSize = 45;

	/** The reserve the upgrade fills, and what a Wall Locker refill tops it up to from then on. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Forge", meta = (ClampMin = 0))
	int32 ReserveAmmo = 180;

	/** The finish of the upgraded copy, or none to keep the weapon's own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Forge")
	TObjectPtr<const URSWeaponSkin> Skin;

	/** What a Wall Locker charges to refill the upgraded copy, instead of the weapon's `RefillCost`. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Forge", meta = (ClampMin = 0))
	int32 RefillCost = 1500;
};

/**
 * A weapon as something a player owns, as opposed to something a pawn has in its hands.
 *
 * Anything that gives the player a weapon — a Wall Locker, the Supply Cache, the Weapon Forge, the
 * debug command — hands over one of these, never an equipment definition. The equipment definition
 * is how the weapon is drawn; this is what is carried in a slot while it is not.
 *
 * Lyra spreads the same fields over instanced fragments. The only thing that goes in an inventory
 * here is a weapon, so fragments would be extension points that nothing extends.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSWeaponItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** What the HUD and the purchase prompts call this weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/** For the weapon slots on the HUD. Soft, because nothing needs it until a HUD is drawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * The finish every copy of this weapon starts with, or none for the mesh's own.
	 *
	 * On the item, not the equipment, because the finish is part of what tells two weapons apart
	 * that are drawn and handled the same way. A copy can still wear another — the Weapon Forge's —
	 * through `URSWeaponItemInstance::SetSkinOverride`.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<const URSWeaponSkin> Skin;

	/**
	 * What the weapon looks like: its receiver and parts, built wherever the weapon is shown.
	 *
	 * On the item rather than on how the weapon is drawn, for the same reason as the skin: two
	 * weapons can be drawn and handled the same way and still look different.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<const URSWeaponAssembly> Assembly;

	/** How the weapon is drawn: instance class, granted abilities, spawned actors. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<URSEquipmentDefinition> EquipmentDefinition;

	/**
	 * Tag stacks a new copy of this weapon starts with: `RS.Weapon.Ammo.Magazine`,
	 * `RS.Weapon.Ammo.Reserve` and `RS.Weapon.MagazineSize`.
	 *
	 * The reserve value is also what a Wall Locker refill tops up to, so "full" is defined once.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "RS.Weapon"))
	TMap<FGameplayTag, int32> InitialStats;

	/**
	 * Scrap to buy this weapon from a Wall Locker when the player does not carry it.
	 *
	 * On the item rather than on the locker, so balancing a weapon is one asset however many
	 * lockers sell it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Locker", meta = (ClampMin = 0))
	int32 PurchaseCost = 1000;

	/** Scrap to top the reserve of a copy the player already carries back up to `InitialStats`. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Locker", meta = (ClampMin = 0))
	int32 RefillCost = 500;

	/** Whether the Weapon Forge takes this weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Forge")
	bool bUpgradable = false;

	/** What the Weapon Forge makes of it. A copy's upgrade level is on the item instance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Forge", meta = (EditCondition = "bUpgradable"))
	FRSWeaponUpgrade Upgrade;
};
