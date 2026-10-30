// Round Survival

#pragma once

#include "CoreMinimal.h"

#include "RSEnemyTypes.generated.h"

/**
 * How fast an enemy moves, as a named step rather than a loose number.
 *
 * The round curve picks a tier, never a speed in centimetres: a round is "these are runners now",
 * and what a runner is stays one entry in the enemy's speed table. It also keeps the animation
 * honest — there are four locomotion looks and a tier is the thing that chooses between them, so a
 * tier nobody can animate is a tier that does not exist.
 *
 * The tier is the **base** speed. Everything that modifies it — a slow, a buff, difficulty scaling
 * — goes through `MoveSpeedMultiplier` on the combat set and multiplies whatever the tier set.
 */
UENUM(BlueprintType)
enum class ERSMoveTier : uint8
{
	/** Drags one leg. The opening rounds, and what the player is meant to walk away from. */
	Shamble,

	/** Still slower than a walking player, but it closes the distance if you stand still. */
	Walk,

	/** Faster than a player's walk, slower than a sprint: the round you stop reloading in the open. */
	Run,

	/** Runs. Still under the player's top speed, because the horde has to stay outrunnable. */
	Sprint
};

/**
 * A piece of an enemy that can come off.
 *
 * The cuts are where the art is cut: the modular meshes come apart at the neck, the shoulders and
 * the hips, so a part is a whole limb and never a forearm. The torso is not a part — it is what
 * the others come off.
 *
 * Also a bit index: `FRSDismemberState::SeveredMask` holds one bit per entry, so the list stays
 * under eight.
 */
UENUM(BlueprintType)
enum class ERSBodyPart : uint8
{
	Head,
	ArmLeft,
	ArmRight,
	LegLeft,
	LegRight,

	MAX UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(ERSBodyPart, ERSBodyPart::MAX);
