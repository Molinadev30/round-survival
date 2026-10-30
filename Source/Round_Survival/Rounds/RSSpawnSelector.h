// Round Survival

#pragma once

#include "UObject/Object.h"

#include "RSSpawnSelector.generated.h"

class APawn;
class ARSZombieSpawner;

/**
 * Decides which eligible spawn point the next enemy comes out of.
 *
 * A policy object rather than a function on the director, because where enemies come from is the
 * single most felt tuning decision in the genre and it is the thing most likely to be swapped:
 * pressure from behind, pressure from the front, a boss round that uses one door. Subclassing
 * this and pointing the round settings at it is the whole of that change.
 *
 * Instanced and editable inline, so the settings asset carries a configured selector rather than
 * a class reference with its numbers somewhere else.
 *
 * Server only. The candidate list is already filtered for eligibility before it arrives.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ROUND_SURVIVAL_API URSSpawnSelector : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @param Candidates  Eligible spawners, never empty.
	 * @param Players     Player pawns still standing. Can be empty when everyone is down.
	 * @return The point to use, or null to release nothing this time.
	 */
	virtual ARSZombieSpawner* SelectSpawner(const TArray<ARSZombieSpawner*>& Candidates, const TArray<APawn*>& Players) const
		PURE_VIRTUAL(URSSpawnSelector::SelectSpawner, return nullptr;);

	//~UObject interface
	virtual UWorld* GetWorld() const override;
	//~End of UObject interface
};

/**
 * The standard policy: the closer a point is to a living player, the likelier it is used.
 *
 * Each point is weighted by the inverse of the straight-line distance to the nearest living
 * player, and the pick is a weighted draw over those weights. No point is ever refused: a
 * threshold would be a cliff, and a wave that stopped arriving because every door fell on the
 * wrong side of a number is worse than one that arrives from somewhere imperfect. Whatever the
 * geometry, the round keeps producing enemies and they keep coming from the near side of the map.
 *
 * Weighted rather than a strict best, so a horde arrives spread out over several doors instead
 * of filing through whichever one is closest for the whole wave.
 *
 * Nothing here asks whether a point is on screen, and nothing traces. Distance is the only input,
 * which means placement costs one length per candidate and behaves the same for every player in
 * a listen server. What keeps a body from appearing on top of somebody is the spawner's own
 * clearance test, not a rule here.
 */
UCLASS(meta = (DisplayName = "Weighted Spawn Selector"))
class ROUND_SURVIVAL_API URSSpawnSelector_Weighted : public URSSpawnSelector
{
	GENERATED_BODY()

public:
	//~URSSpawnSelector interface
	virtual ARSZombieSpawner* SelectSpawner(const TArray<ARSZombieSpawner*>& Candidates, const TArray<APawn*>& Players) const override;
	//~End of URSSpawnSelector interface

protected:
	/** Draw weight for one point. Always positive: this policy refuses nothing. */
	virtual float ScoreSpawner(const ARSZombieSpawner& Spawner, const TArray<APawn*>& Players) const;
};
