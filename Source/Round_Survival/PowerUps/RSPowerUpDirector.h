// Round Survival

#pragma once

#include "UObject/Object.h"

#include "RSPowerUpDirector.generated.h"

class ARSGameMode;
class ARSPowerUpPickup;
class ARSZombieCharacter;
class URSPowerUpDefinition;
class URSPowerUpSettings;
class URSRoundDirector;

/**
 * Decides when a kill drops a power-up, and puts it on the floor. Server only.
 *
 * Lives on the game mode beside the round director, which tells it about every kill through
 * `OnEnemyKilled`; it hears nothing else. Only a player's kill counts: an enemy killed by the
 * debug wipe, or by a power-up that kills everything, pays nobody and drops nothing. The rule's
 * numbers are all in `URSPowerUpSettings`. This class only keeps the two counts the rule needs,
 * drops this round and player kills since the last drop, and the first resets when the round does.
 */
UCLASS()
class ROUND_SURVIVAL_API URSPowerUpDirector : public UObject
{
	GENERATED_BODY()

public:
	void Initialise(ARSGameMode* InGameMode, URSRoundDirector* InRoundDirector, const URSPowerUpSettings* InSettings);

	/** Stops listening. Called as the game mode ends. */
	void Shutdown();

	/**
	 * Puts a power-up on the floor under Location, or nowhere if there is no floor within reach of
	 * it. The drop rule and the debug command both come through here.
	 */
	ARSPowerUpPickup* SpawnPowerUp(const URSPowerUpDefinition* PowerUp, const FVector& Location);

	const URSPowerUpSettings* GetSettings() const { return Settings; }

private:
	void HandleEnemyKilled(ARSZombieCharacter* Enemy, AActor* Killer, bool bHeadshot);
	void HandleRoundNumberChanged(int32 RoundNumber);

	UWorld* GetGameWorld() const;

	TWeakObjectPtr<ARSGameMode> GameMode;
	TWeakObjectPtr<URSRoundDirector> RoundDirector;

	UPROPERTY(Transient)
	TObjectPtr<const URSPowerUpSettings> Settings;

	int32 DropsThisRound = 0;

	/** Starts high, so the first kill of the match can already drop one. */
	int32 KillsSinceLastDrop = MAX_int32;

	FDelegateHandle EnemyKilledHandle;
	FDelegateHandle RoundNumberHandle;
};
