// Round Survival

#pragma once

#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"

#include "RSGameMode.generated.h"

class ARSPlayerState;
class ARSZombieCharacter;
class UGameplayEffect;
class URSAbilitySystemComponent;
class URSPowerUpDirector;
class URSPowerUpSettings;
class URSRoundDirector;
class URSRoundSettings;

/**
 * Game mode. Server only, by definition.
 *
 * Owns the round director and the settings asset that feeds it, and wires the framework classes
 * together so that every map gets the right game state, player state and controller without
 * anyone remembering to set them in World Settings.
 *
 * It also owns the players' lives between pawns. It watches every player's RS.State.Downed and
 * RS.State.Dead: a player who dies loses their pawn and watches a teammate until the round ends,
 * when they are put back; and the moment nobody is left standing, the match is over.
 *
 * A map that leaves `RoundSettings` empty simply never starts a round, which is what the weapon
 * bench wants: enemies placed by hand, no quota running underneath them.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARSGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	URSRoundDirector* GetRoundDirector() const { return RoundDirector; }

	/** Null on a map without rounds or without power-up settings. */
	URSPowerUpDirector* GetPowerUpDirector() const { return PowerUpDirector; }

	/**
	 * Brings every dead player back: the death state comes off, RespawnEffect puts their health
	 * back, and they get a new pawn — with the starting weapons, since what they carried went with
	 * the old one. Their scrap was never on the pawn. Called by the round director as a round ends.
	 */
	void RespawnDeadPlayers();

	/**
	 * A player asked to restart a lost match. Only the host may, and only once the match is over;
	 * anyone else is ignored.
	 */
	void RequestRestartMatch(const APlayerController* Requester);

	/** Starts the map over from round one, keeping whoever is connected. The debug command calls this directly. */
	void RestartMatch();

	//~AGameModeBase interface
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/**
	 * Who a dead player may watch: the engine's spectator cycle (APlayerController::ViewAPlayer)
	 * asks this of every candidate, after checking they have a pawn. A teammate still in play, then,
	 * standing or downed: a dead player's pawn is already gone.
	 */
	virtual bool CanSpectate_Implementation(APlayerController* Viewer, APlayerState* ViewTarget) override;
	//~End of AGameModeBase interface

protected:
	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	/** The difficulty curve this map plays. Empty means no rounds run at all. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Round")
	TObjectPtr<URSRoundSettings> RoundSettings;

	/** Which power-ups kills drop, and when. Empty means none ever do. Needs round settings: the drops come from the rounds' kills. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Power-Ups")
	TObjectPtr<const URSPowerUpSettings> PowerUpSettings;

	/** Applied to a dead player as they come back, after the death state is taken off: full health. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player")
	TSubclassOf<UGameplayEffect> RespawnEffect;

	/**
	 * Created here rather than placed in the level: the director is rules, not an actor, and it
	 * has nothing to replicate. Clients read the round off the game state.
	 */
	UPROPERTY(Transient)
	TObjectPtr<URSRoundDirector> RoundDirector;

	/** Beside the round director and for the same reason: rules, not an actor. */
	UPROPERTY(Transient)
	TObjectPtr<URSPowerUpDirector> PowerUpDirector;

private:
	/** Starts listening to one player's downed and dead tags. */
	void WatchPlayer(ARSPlayerState* PlayerState);
	void UnwatchPlayer(ARSPlayerState* PlayerState);

	void HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount, TWeakObjectPtr<ARSPlayerState> PlayerState);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount, TWeakObjectPtr<ARSPlayerState> PlayerState);

	/**
	 * Takes a dead player's pawn away and makes them a spectator. Run on the next frame, not from
	 * inside the tag change: the tag moves while an effect is being applied, and the pawn being
	 * destroyed under that would leave the ability system mid-application on an avatar that is gone.
	 */
	void RetirePlayer(TWeakObjectPtr<ARSPlayerState> PlayerState);

	/** Credits a player's kill to their scoreboard row. Kills nobody is credited with count for no one. */
	void HandleEnemyKilled(ARSZombieCharacter* Enemy, AActor* Killer, bool bHeadshot);

	FDelegateHandle EnemyKilledHandle;

	/**
	 * Whether this player can still keep the match going: on their feet, or downed and getting back
	 * up on their own (RS.State.SelfReviving). Neither bleeding out nor dead.
	 */
	static bool IsPlayerStanding(const ARSPlayerState* PlayerState);

	/** Ends the match if nobody is standing. Asked whenever a player goes down, dies or leaves. */
	void CheckForMatchLost();

	/**
	 * During a restart, switches the map as soon as no remote client is left, rather than waiting out
	 * the net driver's ServerTravelPause. A client only closes its connection to travel once the
	 * reliable travel order has reached it, so when the last one has gone every client has it; the
	 * pause stays as the ceiling for a client the order never reaches. Leaving is the leaving
	 * controller, still in the world's list while its logout runs.
	 */
	void SwitchLevelOnceClientsLeft(const AController* Leaving = nullptr);

	/** Each watched player's ability system, and the bindings to take off it again. */
	struct FWatchedPlayer
	{
		TWeakObjectPtr<URSAbilitySystemComponent> AbilitySystem;
		FDelegateHandle DownedHandle;
		FDelegateHandle DeadHandle;
	};

	TMap<TWeakObjectPtr<ARSPlayerState>, FWatchedPlayer> WatchedPlayers;
};
