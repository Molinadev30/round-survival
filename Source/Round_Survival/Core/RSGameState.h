// Round Survival

#pragma once

#include "GameFramework/GameStateBase.h"

#include "RSGameState.generated.h"

class URSPowerUpDefinition;

/**
 * Where a round currently is.
 *
 *   Intermission --(timer)--> Spawning --(quota spent)--> Cleanup --(none alive)--> Intermission
 *
 * and from any of them to GameOver when nobody is left standing.
 */
UENUM(BlueprintType)
enum class ERSRoundPhase : uint8
{
	/** Before the match starts. */
	WaitingToStart,

	/** Breathing room between rounds. Repairs and purchases happen here. */
	Intermission,

	/** The round's quota is being released. */
	Spawning,

	/** Quota exhausted; waiting for the survivors of the wave to be killed. */
	Cleanup,

	/** Nobody is left standing. Stays here until the host restarts the match. */
	GameOver,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FRSRoundNumberChanged, int32 /*RoundNumber*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FRSRoundPhaseChanged, ERSRoundPhase /*NewPhase*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FRSZombieCountChanged, int32 /*Alive*/, int32 /*RemainingToSpawn*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FRSPlayerStateListChanged, APlayerState* /*PlayerState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FRSPowerChanged, bool /*bPowerOn*/);

/**
 * Observable match state.
 *
 * Holds what every client needs to render; it holds no rules. The round director on the game
 * mode writes the round, the Power Core writes the power, and both only on the server.
 *
 * Nothing polls this on tick — the HUD binds to the delegates below.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARSGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	int32 GetRoundNumber() const { return RoundNumber; }

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	ERSRoundPhase GetRoundPhase() const { return RoundPhase; }

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	int32 GetZombiesAlive() const { return ZombiesAlive; }

	UFUNCTION(BlueprintPure, Category = "RS|Round")
	int32 GetZombiesRemainingToSpawn() const { return ZombiesRemainingToSpawn; }

	/** Server only. Called by the round director. */
	void SetRoundNumber(int32 InRoundNumber);
	void SetRoundPhase(ERSRoundPhase InPhase);
	void SetZombieCounts(int32 InAlive, int32 InRemainingToSpawn);

	FRSRoundNumberChanged OnRoundNumberChanged;
	FRSRoundPhaseChanged OnRoundPhaseChanged;
	FRSZombieCountChanged OnZombieCountChanged;

	/**
	 * Whether the map's power is on. One switch for the whole map, kept here rather than on the
	 * Power Core so that what needs power asks the match and never has to find the core.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Power")
	bool IsPowerOn() const { return bPowerOn; }

	/** Server only. Called by the Power Core. */
	void SetPowerOn(bool bInPowerOn);

	/** Fires on every machine when the power changes, the server included. */
	FRSPowerChanged OnPowerChanged;

	/**
	 * Whether exactly one player is in the match, spectators-only aside. Read from PlayerArray, which
	 * the engine keeps on every machine. Second Wind's self-revive asks it on the server.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Match")
	bool IsSoloMatch() const;

	/**
	 * A player joined or left PlayerArray, on any machine. The engine keeps that array in step on
	 * clients too — each player state adds itself as it replicates in — but says nothing when it
	 * does, and the HUD's scrap list has to hear about it.
	 */
	FRSPlayerStateListChanged OnPlayerStateAdded;
	FRSPlayerStateListChanged OnPlayerStateRemoved;

	/**
	 * Server. Someone picked up PowerUp: every machine plays its pickup sound. Unreliable, like the
	 * scrap popups: a lost one costs a sound, never the power-up, which the server has already applied.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPowerUpCollected(const URSPowerUpDefinition* PowerUp);

	//~AGameStateBase interface
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	//~End of AGameStateBase interface

protected:
	UFUNCTION()
	void OnRep_RoundNumber();

	UFUNCTION()
	void OnRep_RoundPhase();

	UFUNCTION()
	void OnRep_ZombieCounts();

	UFUNCTION()
	void OnRep_PowerOn();

	//~AActor interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface

private:
	UPROPERTY(ReplicatedUsing = OnRep_RoundNumber)
	int32 RoundNumber = 0;

	UPROPERTY(ReplicatedUsing = OnRep_RoundPhase)
	ERSRoundPhase RoundPhase = ERSRoundPhase::WaitingToStart;

	UPROPERTY(ReplicatedUsing = OnRep_ZombieCounts)
	int32 ZombiesAlive = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ZombieCounts)
	int32 ZombiesRemainingToSpawn = 0;

	UPROPERTY(ReplicatedUsing = OnRep_PowerOn)
	bool bPowerOn = false;
};
