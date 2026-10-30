// Round Survival

#pragma once

#include "Core/RSGameState.h"
#include "UObject/Object.h"

#include "RSRoundDirector.generated.h"

class ARSGameMode;
class ARSGameState;
class ARSZombieCharacter;
class URSRoundSettings;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FRSEnemyKilled, ARSZombieCharacter* /*Enemy*/, AActor* /*Killer*/, bool /*bHeadshot*/);

/**
 * The round flow: a plain state machine, on the server, owned by the game mode.
 *
 *   Intermission --(timer)--> Spawning --(quota released)--> Cleanup --(none alive)--> Intermission
 *
 *   any phase --(nobody standing)--> GameOver --(host restarts the match)--> a new match
 *
 * Deliberately not a StateTree. Three linear states driven by two timers and two counters gain
 * nothing from an asset indirection, and this is the part of the game that has to be readable at
 * a glance when a round wedges and nothing is spawning.
 *
 * **A round ends when its last enemy dies, not when its last enemy spawns.** That is what makes
 * the genre's rhythm work — the round is a room you have to clear, not a timer you have to
 * survive — and it has a quiet consequence the rest of the code depends on: no enemy is ever
 * alive across a round boundary, so scaling only ever applies to a pawn that was born this round
 * and nothing has to be re-scaled in place.
 *
 * Every number comes from `URSRoundSettings`. This class holds the rules; the asset holds the
 * curve. What it writes, it writes through `ARSGameState`, which is the only thing clients see.
 */
UCLASS()
class ROUND_SURVIVAL_API URSRoundDirector : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * An enemy the director released has died, killed by Killer: a player's state for a player's
	 * kill, the enemy itself for the debug wipe. Server only. The power-up director's one input, and
	 * where the game mode counts each player's kills for the scoreboard.
	 */
	FRSEnemyKilled OnEnemyKilled;

	/** Binds the director to its game mode and settings. Called once, on the server. */
	void Initialise(ARSGameMode* InGameMode, URSRoundSettings* InSettings);

	/** Opens the first intermission. Nothing happens until this is called. */
	UFUNCTION(BlueprintCallable, Category = "RS|Round")
	void StartMatch();

	/** Stops both timers and leaves the counters where they are. */
	UFUNCTION(BlueprintCallable, Category = "RS|Round")
	void StopMatch();

	/**
	 * Nobody is left standing: the match is lost and stops where it is.
	 *
	 * The game mode decides when, since it is the one watching the players; this only stops the
	 * round. No timer moves on from here — the match stays over until the host restarts it.
	 */
	void EnterGameOver();

	/**
	 * Jumps straight to a round's intermission, clearing whatever is alive. The debug hook.
	 *
	 * Goes through the same entry as a normal round rather than writing the counters directly,
	 * so a forced round exercises the code a played one does. Refused once the match is over.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Round")
	void SkipToRound(int32 NewRoundNumber);

	/**
	 * Queues enemies to be released outside the quota, for the horde measurement.
	 *
	 * Queued rather than released on the spot: spawn points refuse to fire twice inside their
	 * cooldown, so asking for sixty-four in one frame would produce one per point and a command
	 * that quietly did a fifteenth of what it said. This drains as fast as the points allow.
	 *
	 * @return How many were added to the queue.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Round")
	int32 SpawnHorde(int32 Count);

	/** Enemies still queued by SpawnHorde. */
	UFUNCTION(BlueprintPure, Category = "RS|Round")
	int32 GetHordeRemaining() const { return HordeRemaining; }

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	int32 GetRoundNumber() const { return RoundNumber; }

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	ERSRoundPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	int32 GetAliveCount() const { return LiveEnemies.Num(); }

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	int32 GetRemainingToSpawn() const { return RemainingToSpawn; }

	//~UObject interface
	virtual UWorld* GetWorld() const override;
	//~End of UObject interface

protected:
	void EnterIntermission();
	void EnterSpawning();
	void EnterCleanup();

	/** One release attempt. The repeating half of the Spawning state. */
	void SpawnTick();

	/** Drains the debug horde queue, independently of the round's own quota. */
	void HordeTick();

	/** Drops whatever the horde queue still had waiting. */
	void AbandonHorde();

	/** Puts one enemy into the world and stamps this round's health and speed on it. */
	ARSZombieCharacter* ReleaseOneEnemy();

	/** Applies the round's health effect and speed tier to a freshly spawned enemy. */
	void ApplyRoundScaling(ARSZombieCharacter& Enemy) const;

	void HandleEnemyDied(ARSZombieCharacter* Enemy, AActor* Killer, bool bHeadshot);

	/**
	 * An enemy leaving the world by any other route than death: pooled alive, or destroyed by a
	 * travel or a full pool.
	 *
	 * Death is the expected way a body leaves, but it is not the only one, and a round that only
	 * counted deaths would wait for ever on an enemy that was removed rather than killed.
	 */
	void HandleEnemyReturnedToPool(ARSZombieCharacter* Enemy);

	UFUNCTION()
	void HandleEnemyDestroyed(AActor* DestroyedActor);

	/** Drops an enemy from the live list and stops watching it, along with anything else that has gone stale. */
	void ForgetEnemy(ARSZombieCharacter* Enemy);

	/** Unbinds from an enemy's death, pool return and destruction. */
	void StopWatching(ARSZombieCharacter* Enemy);

	/** Ends the round if cleanup is what is waiting on the body that just left. */
	void CheckRoundComplete();

	/** Moves the phase and mirrors it onto the game state in one place. */
	void SetPhase(ERSRoundPhase NewPhase);

	/** Pushes the counters to the game state. Called whenever either one moves. */
	void PublishCounts();

	/** Connected players, for the quota and the ceiling. Not living players: a downed player's
	 *  share of the horde does not disappear while they are waiting to be revived. */
	int32 GetPlayerCount() const;

	ARSGameState* GetRSGameState() const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ARSGameMode> GameMode;

	UPROPERTY(Transient)
	TObjectPtr<URSRoundSettings> Settings;

	int32 RoundNumber = 0;

	ERSRoundPhase Phase = ERSRoundPhase::WaitingToStart;

	/** Still to be released this round. */
	int32 RemainingToSpawn = 0;

	/**
	 * What this director released and has not buried yet. Enemies placed by hand in a level are
	 * not in here and never hold a round open.
	 *
	 * A list rather than a counter because a forced round has to be able to clear the board, and
	 * weak so an enemy removed by anything other than dying — a travel, a `KillAll` — leaves a
	 * null rather than a dangling count.
	 */
	TArray<TWeakObjectPtr<ARSZombieCharacter>> LiveEnemies;

	/** Outstanding debug horde, released as fast as the spawn points will allow. */
	int32 HordeRemaining = 0;

	FTimerHandle PhaseTimerHandle;
	FTimerHandle SpawnTimerHandle;
	FTimerHandle HordeTimerHandle;
};
