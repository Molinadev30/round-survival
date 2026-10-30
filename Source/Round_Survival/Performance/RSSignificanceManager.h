// Round Survival

#pragma once

#include "Performance/RSSignificanceSettings.h"
#include "SignificanceManager.h"

#include "RSSignificanceManager.generated.h"

class ARSZombieCharacter;

/**
 * Decides, on the server, how much each enemy matters to the players, and has the enemy spend accordingly.
 *
 * **Every player's view, not the host's.** The engine's own visibility-based savings ask whether the mesh was
 * rendered on this machine, and on a listen server that machine's renderer is only the host's view. An enemy a client
 * is looking at but the host is not would stop refreshing its bones there, and the server validates the client's
 * shots against exactly that pose. So significance is worked out from every player controller's view point here, and
 * the engine keeps the highest over all of them: an enemy is High if *any* player sees it or is close to it.
 *
 * **Measured first, and after.** Two thirds of an enemy's game-thread time is its skeletal mesh — animation, the
 * post-evaluation pass and moving the physics bodies its hit zones live on. An enemy nobody sees
 * keeps only its montages, the gameplay part of its animation. Its movement is left alone: spacing out a far enemy's
 * movement was built, measured at about 6 µs an enemy net on this map, and taken out again. Tuning is
 * URSSignificanceSettings.
 *
 * Updated on a timer, not every frame: a tier is a coarse fact, and the view cone's margin covers the turn a player
 * can make in between. Server only; clients rely on the engine's rendered test for their copies, which is the right
 * one there. `RS.Significance.Enabled 0` puts every enemy back to full updates, to compare; `RS.Significance.Draw 1`
 * labels each enemy with its tier.
 */
UCLASS()
class ROUND_SURVIVAL_API URSSignificanceManager : public USignificanceManager
{
	GENERATED_BODY()

public:
	URSSignificanceManager();

	/** This world's manager, or null where it does not run (clients) or is not the configured class. */
	static URSSignificanceManager* Get(const UObject* WorldContextObject);

	/** Starts managing an enemy that has entered the world. Server only; registering twice changes nothing. */
	void RegisterEnemy(ARSZombieCharacter* Enemy);

	/** Stops managing an enemy leaving the world, dying or pooled. */
	void UnregisterEnemy(ARSZombieCharacter* Enemy);

	/** RS.Significance.Enabled. */
	static bool IsEnabled();

	static ERSSignificanceTier ToTier(float Significance);

private:
	/** The timer's work: every player's view point, then the engine's update, which re-tiers every enemy. */
	void UpdateFromPlayers();

	/** One enemy's significance for one view point. Runs on worker threads: reads positions, writes nothing. */
	static float CalculateSignificance(USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& Viewpoint);

	/** Hands an enemy its tier after an update. Game thread; the enemy does nothing unless the tier changed. */
	static void ApplySignificance(USignificanceManager::FManagedObjectInfo* ObjectInfo, float OldSignificance, float NewSignificance, bool bFinal);

	/** Puts every managed enemy on one tier: High, when significance is switched off. */
	void ApplyTierToAll(ERSSignificanceTier Tier);

	FTimerHandle UpdateTimerHandle;
	bool bWasEnabled = true;
};
