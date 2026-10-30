// Round Survival

#pragma once

#include "CoreMinimal.h"

/**
 * One switch for every piece of enemy debug drawing.
 *
 * `RS.Enemy.Debug 1` on the console turns on the melee sweep, the frontal cone, the line of sight
 * trace, the current target and the attack range, all at once and without touching an asset or
 * recompiling. Tuning reach against an animation means watching all of them together: a swing that
 * misses tells you nothing on its own, because the sphere, the cone and the trace each reject for
 * their own reason and they look identical from outside.
 *
 * Everything here compiles out where DrawDebug does.
 */
namespace RSEnemyDebug
{
	/** Whether enemy debug drawing is on. Always false where debug drawing is compiled out. */
	ROUND_SURVIVAL_API bool IsEnabled();

	/** Colours shared by the drawing sites, so a passing test always looks the same. */
	ROUND_SURVIVAL_API FColor PassColor();
	ROUND_SURVIVAL_API FColor FailColor();

	/**
	 * Draws a flat cone on the ground, opening from Origin along Direction.
	 *
	 * @param HalfAngleDegrees  Half the opening, matching the melee ability's own parameter.
	 */
	ROUND_SURVIVAL_API void DrawGroundCone(const UWorld* World, const FVector& Origin, const FVector& Direction,
		float Radius, float HalfAngleDegrees, const FColor& Color, float Lifetime);

	/**
	 * Draws what one enemy is doing this frame: the state it is in, who it is after and how far
	 * away that is by road.
	 *
	 * Called from the tasks that tick in each state rather than from one place, because there is
	 * no single thing that ticks for every enemy every frame and adding one only for debugging
	 * would be a tick per enemy paid for all the time. The corollary is that a state with no
	 * ticking task draws nothing, which is itself worth noticing.
	 *
	 * @param PathDistance  Travel distance to the target, or negative when it was never measured.
	 */
	ROUND_SURVIVAL_API void DrawEnemyState(const APawn* Pawn, const AActor* Target, const TCHAR* StateName, float PathDistance);
}
