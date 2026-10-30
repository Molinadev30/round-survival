// Round Survival

#pragma once

#include "UObject/Interface.h"

#include "RSAbilitySourceInterface.generated.h"

class UPhysicalMaterial;
struct FGameplayTagContainer;

UINTERFACE(MinimalAPI)
class URSAbilitySourceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * Whatever a damage calculation should ask about attenuation — in practice, the weapon.
 *
 * The damage execution runs on the server with no idea what fired the shot. It reads the source
 * object off the effect context and asks it these two questions, so a rifle with falloff and a
 * melee hit with none go through the same execution without a branch per weapon type.
 */
class IRSAbilitySourceInterface
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * Multiplier on base damage for how far the shot travelled.
	 *
	 * @param Distance  Centimetres from the muzzle to the impact.
	 */
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr) const = 0;

	/**
	 * Multiplier on base damage for what was hit.
	 *
	 * This is the headshot: the physical material on the enemy's physics asset carries a hit-zone
	 * tag and the weapon maps that tag to a multiplier. Bone names never enter into it.
	 */
	virtual float GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const = 0;
};
