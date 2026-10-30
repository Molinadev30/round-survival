// Round Survival

#pragma once

#include "UObject/ObjectPtr.h"

#include "RSLeftHandGrip.generated.h"

class UAnimSequenceBase;

/**
 * Where the left hand holds a long gun.
 *
 * A closed set: the animations author exactly these three holds for the rifle, and a new one
 * would need new poses before it needed a new name. A weapon's grip is not chosen by the player but
 * follows from what is bolted on: a foregrip part declares its grip (see FRSWeaponPart::LeftHandGrip),
 * and a weapon without one is held by the handguard.
 */
UENUM(BlueprintType)
enum class ERSLeftHandGrip : uint8
{
	/** Palm under the handguard, for a weapon with no foregrip. The base pose of every rifle animation, so it needs no pose of its own. */
	Handguard,

	/** Fist around a vertical foregrip. */
	Vertical,

	/** Hand resting on an angled foregrip. */
	Angled
};

/**
 * The poses that put the left hand on one grip, per perspective and per stance.
 *
 * Single-frame poses rather than animations, layered over the left arm on top of whatever the body
 * is doing; the arm is then pulled back onto the weapon by IK. The aim pose is for aiming down
 * sights, which does not exist yet, and falls back to the idle one while empty.
 */
USTRUCT(BlueprintType)
struct FRSLeftHandGripPoses
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	TObjectPtr<UAnimSequenceBase> FirstPersonIdle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	TObjectPtr<UAnimSequenceBase> FirstPersonAim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	TObjectPtr<UAnimSequenceBase> ThirdPersonIdle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	TObjectPtr<UAnimSequenceBase> ThirdPersonAim;
};
