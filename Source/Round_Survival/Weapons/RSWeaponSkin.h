// Round Survival

#pragma once

#include "Engine/DataAsset.h"

#include "RSWeaponSkin.generated.h"

class UMaterialInterface;
class UMeshComponent;

/**
 * A finish for a weapon: one material that repaints every part of it, whatever the part.
 *
 * The weapon art textures each part of a weapon separately — the receiver, the handguard and the
 * sights each have their own base, ORM and normal maps — so a skin cannot be one material swapped
 * in everywhere. Instead the skin names a parent material, and each slot that takes the skin gets a
 * dynamic instance of it carrying that slot's own textures, copied over from the material the mesh
 * came with. The camouflage goes on top of the part's own wear and detail rather than replacing it,
 * and one skin asset dresses any weapon built on the same master material.
 *
 * A slot takes the skin only if its original material has every texture this skin copies. That is
 * what keeps a scope's glass or a transparent magazine's shell looking like glass: their materials
 * have no base colour map to carry over. Parts that should stay unpainted even though their
 * material would qualify — the rounds — opt out on the weapon assembly instead.
 *
 * Which skin a weapon shows is decided by its item instance, not here: see
 * `URSWeaponItemInstance::GetSkin`.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSWeaponSkin : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** For the HUD and the Weapon Forge's prompt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/**
	 * What every reskinned slot becomes an instance of. Normally a material instance of
	 * `M_RS_WeaponCamo`, which holds the pattern, its colours and, for the rarer skins, its motion.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
	TObjectPtr<UMaterialInterface> Material;

	/** Texture parameters carried from each slot's original material onto its skinned instance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
	TArray<FName> CopiedTextureParameters = { TEXT("Texture Base"), TEXT("Texture ORM"), TEXT("Texture Normal") };

	/**
	 * Puts Skin on every slot of Component that can take it, or the mesh's own materials back when
	 * Skin is null. Reapplying replaces the previous skin rather than stacking on it.
	 */
	static void ApplyToComponent(const URSWeaponSkin* Skin, UMeshComponent* Component);

private:
	/** Whether a slot's original material carries everything this skin needs from it. */
	bool CanReskin(const UMaterialInterface* Original) const;
};
