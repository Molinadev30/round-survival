// Round Survival

#pragma once

#include "GameplayTagContainer.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "RSPhysicalMaterialWithTags.generated.h"

/**
 * A physical material that carries gameplay tags.
 *
 * This is how hit zones work: the enemy's physics asset assigns this material per body, tagged
 * `RS.Damage.Zone.Head`, `.Torso` or `.Limb`, and the weapon maps those tags to damage
 * multipliers. Matching on bone names instead would put the skeleton's naming convention into
 * combat code and break the moment an enemy uses a different rig.
 */
UCLASS()
class ROUND_SURVIVAL_API URSPhysicalMaterialWithTags : public UPhysicalMaterial
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PhysicalProperties", meta = (Categories = "RS.Damage.Zone"))
	FGameplayTagContainer Tags;
};
