// Round Survival

#pragma once

#include "Chaos/ChaosEngineInterface.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "RSImpactEffects.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UPhysicalMaterial;
class USoundBase;

/** What a bullet leaves where it lands on one kind of surface. Any of it may be empty. */
USTRUCT(BlueprintType)
struct FRSImpactEffect
{
	GENERATED_BODY()

	/** Spawned at the impact, its X axis along the surface normal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<USoundBase> Sound;

	/** The hole, projected into the surface. Leave it empty on anything that moves, such as an enemy: a decal stays where it was put. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UMaterialInterface> Decal;

	/** Width and height of the hole. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (EditCondition = "Decal", ClampMin = 0.0, ForceUnits = cm))
	float DecalSize = 8.0f;
};

/**
 * Which impact each surface gets, looked up from the physical material the shot touched.
 *
 * A hit zone comes first: an enemy's bodies carry `RS.Damage.Zone.*` on their
 * URSPhysicalMaterialWithTags, so a head can bleed more than a limb without a surface type per
 * zone. Then the material's surface type, as the project names them. Anything else, including a
 * mesh with no physical material at all, gets Default.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSImpactEffects : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FRSImpactEffect& FindEffect(const UPhysicalMaterial* PhysicalMaterial) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Impact", meta = (Categories = "RS.Damage.Zone"))
	TMap<FGameplayTag, FRSImpactEffect> HitZones;

	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TMap<TEnumAsByte<EPhysicalSurface>, FRSImpactEffect> Surfaces;

	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	FRSImpactEffect Default;
};
