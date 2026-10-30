// Round Survival

#pragma once

#include "AbilitySystem/RSAbilitySet.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"

#include "RSPlayerState.generated.h"

class URSAbilitySystemComponent;
class URSCombatSet;
class URSEconomySet;
class URSHandlingSet;
class URSHealthSet;

DECLARE_MULTICAST_DELEGATE_OneParam(FRSPlayerNameChanged, class ARSPlayerState* /*PlayerState*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FRSScrapDeltaReceived, int32 /*Delta*/, const FGameplayTag& /*Reason*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FRSBleedOutClockChanged, class ARSPlayerState* /*PlayerState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FRSMatchStatsChanged, class ARSPlayerState* /*PlayerState*/);

struct FGameplayEffectSpec;

/**
 * What a player did this match, for the end-of-match scoreboard. The score is not in here: it is the
 * engine's APlayerState::Score, which already replicates to everyone.
 *
 * Counted on the server only, each where the thing happens, and never reset: a new match is a new
 * level, and a new level is a new player state.
 */
USTRUCT(BlueprintType)
struct FRSPlayerMatchStats
{
	GENERATED_BODY()

	/** Enemies this player killed, headshots included. A Purge or a debug wipe credits nobody. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Stats")
	int32 Kills = 0;

	/** Of those, the ones whose killing shot landed on the head. A melee kill never is one. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Stats")
	int32 Headshots = 0;

	/** Times this player went down, Second Wind's solo self-revives included. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Stats")
	int32 Downs = 0;

	/** Teammates this player picked back up. Getting yourself up does not count. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Stats")
	int32 Revives = 0;
};

/**
 * A downed player's bleed-out, as every machine can read it: when it runs out, or how much is left
 * while a revive holds it. Published by the downed ability, which owns the real clock on the server.
 *
 * A moment on the server's clock rather than a number of seconds counting down, so nothing has to
 * be sent while it runs: a client measures it against AGameStateBase::GetServerWorldTimeSeconds,
 * which the engine already keeps in step with the server's.
 */
USTRUCT(BlueprintType)
struct FRSBleedOutClock
{
	GENERATED_BODY()

	/** Seconds the whole bleed-out lasts. Zero when the player is not bleeding out. */
	UPROPERTY()
	float Duration = 0.0f;

	/** Server world time the bleed-out runs out at, while it runs. */
	UPROPERTY()
	double EndServerTime = 0.0;

	/** Seconds left while a revive holds the clock; negative while it runs. */
	UPROPERTY()
	float PausedSecondsLeft = -1.0f;

	/**
	 * The clock counts to getting back up rather than to bleeding out: Second Wind, in a solo match.
	 * Same clock, so the HUD only changes what it calls it.
	 */
	UPROPERTY()
	bool bSelfRevive = false;

	bool IsActive() const { return Duration > 0.0f; }
	bool IsPaused() const { return PausedSecondsLeft >= 0.0f; }

	/** Seconds left at ServerTime, never below zero. Zero when not active. */
	float GetSecondsLeft(double ServerTime) const
	{
		if (!IsActive())
		{
			return 0.0f;
		}

		return IsPaused() ? PausedSecondsLeft : static_cast<float>(FMath::Max(0.0, EndServerTime - ServerTime));
	}
};

/**
 * Player state. Hosts the player's ability system.
 *
 * The ability system component lives here rather than on the pawn so that scrap, health limits
 * and any active buff survive the character being destroyed and respawned — losing your balance
 * because you went down is not a design decision anyone would sign off on.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARSPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "RS|Player")
	URSAbilitySystemComponent* GetRSAbilitySystemComponent() const { return AbilitySystemComponent; }

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	const URSHealthSet* GetHealthSet() const { return HealthSet; }
	const URSCombatSet* GetCombatSet() const { return CombatSet; }
	const URSEconomySet* GetEconomySet() const { return EconomySet; }
	const URSHandlingSet* GetHandlingSet() const { return HandlingSet; }

	/** The player's name changed, on any machine that can show it. The HUD's scrap list labels rows with it. */
	FRSPlayerNameChanged OnPlayerNameChanged;

	/**
	 * One award or one purchase moved this player's scrap, by Delta, for Reason — an RS.Economy tag,
	 * or none. Fired on the owning machine only, once per change, so two awards in one frame arrive
	 * as two. Drives the HUD's "+60" / "-1,000" popup; the balance itself comes by replication.
	 */
	FRSScrapDeltaReceived OnScrapDeltaReceived;

	/**
	 * Scrap this player lost going down, owed to whoever revives them. Server only: the downed
	 * ability records it, a completed revive pays it out, and bleeding out forfeits it.
	 */
	void SetScrapLostToDown(int32 Amount) { ScrapLostToDown = Amount; }

	/** What is owed to a reviver, which is then no longer owed. */
	int32 TakeScrapLostToDown()
	{
		const int32 Amount = ScrapLostToDown;
		ScrapLostToDown = 0;
		return Amount;
	}

	/**
	 * Server only. The downed ability publishes its clock here whenever it starts, stops, resumes or
	 * ends; a default clock means the player is not bleeding out.
	 */
	void SetBleedOutClock(const FRSBleedOutClock& InClock);

	const FRSBleedOutClock& GetBleedOutClock() const { return BleedOutClock; }

	/**
	 * The bleed-out clock changed, on every machine: started, held by a revive, resumed or over. Not
	 * fired as it runs — read the clock against the server's time for that.
	 */
	FRSBleedOutClockChanged OnBleedOutClockChanged;

	/** Server only. One kill credited to this player; bHeadshot when the killing shot was one. */
	void RecordKill(bool bHeadshot);

	/** Server only. This player went down. */
	void RecordDown();

	/** Server only. This player picked a teammate back up. */
	void RecordRevive();

	UFUNCTION(BlueprintPure, Category = "RS|Stats")
	FRSPlayerMatchStats GetMatchStats() const { return MatchStats; }

	/** Scrap earned this match. The score's only source; see HandleScrapAwarded. */
	UFUNCTION(BlueprintPure, Category = "RS|Stats")
	int32 GetMatchScore() const { return FMath::RoundToInt(GetScore()); }

	/**
	 * The stats or the score changed, on every machine. The scoreboard binds this rather than
	 * reading the numbers on tick.
	 */
	FRSMatchStatsChanged OnMatchStatsChanged;

	//~APlayerState interface
	virtual void OnRep_PlayerName() override;
	virtual void OnRep_Score() override;
	//~End of APlayerState interface

	//~AActor interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

private:
	UFUNCTION()
	void OnRep_BleedOutClock();

	/**
	 * To everyone, not just the owner: teammates read it too. It cannot ride on a gameplay effect's
	 * duration, because the ability system here replicates effects to their owner alone (Mixed), and
	 * a duration cannot be held while a revive is under way.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_BleedOutClock)
	FRSBleedOutClock BleedOutClock;

	UFUNCTION()
	void OnRep_MatchStats();

	/** To everyone, not just the owner: the scoreboard shows every player's row. */
	UPROPERTY(ReplicatedUsing = OnRep_MatchStats)
	FRSPlayerMatchStats MatchStats;

	/** After a server-side write: the listen server's own scoreboard gets no rep notify. */
	void MatchStatsChanged();

	/** Grants DefaultAbilitySets. Server only, and only once. */
	void GrantDefaultAbilitySets();

	/** Server: an executed effect changed Scrap. Tells the owner by how much and why. */
	void HandleScrapChanged(AActor* Instigator, AActor* Causer, const FGameplayEffectSpec* EffectSpec, float Magnitude, float OldValue, float NewValue);

	/**
	 * Server: scrap was awarded. Every award adds to the score, after the income multiplier, except a
	 * revive's: that is a teammate's lost scrap changing hands, not scrap earned. Spending never comes
	 * through here, and neither does a Supply Cache refund, which writes Scrap directly.
	 */
	void HandleScrapAwarded(AActor* Instigator, AActor* Causer, const FGameplayEffectSpec* EffectSpec, float Magnitude, float OldValue, float NewValue);

	/**
	 * Unreliable on purpose: the popup is decoration. A lost one costs a "+10" on screen, never a
	 * point of scrap — the balance replicates on its own.
	 */
	UFUNCTION(Client, Unreliable)
	void ClientScrapChanged(int32 Delta, FGameplayTag Reason);

	FDelegateHandle ScrapChangedHandle;
	FDelegateHandle ScrapAwardedHandle;

	/** See SetScrapLostToDown. Server only. */
	int32 ScrapLostToDown = 0;

	UPROPERTY(VisibleAnywhere, Category = "RS|Player")
	TObjectPtr<URSAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * The kit every player starts with: interact, reload, whatever is not tied to a weapon.
	 *
	 * Granted here and not on the pawn because the ability system lives here — a grant on the pawn
	 * would be lost on respawn, and re-granting per pawn would stack passives. Weapons grant their
	 * own sets on equip and revoke them on unequip; those never come through this list.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Abilities")
	TArray<TObjectPtr<const URSAbilitySet>> DefaultAbilitySets;

	/** What the grant handed out, so EndPlay can take it all back. */
	UPROPERTY()
	FRSAbilitySet_GrantedHandles DefaultAbilityHandles;

	/**
	 * Attribute sets are created here as default subobjects rather than granted by an ability
	 * set, because a player without health or a balance is never a valid state to be in, even
	 * for the frame before a grant lands.
	 */
	UPROPERTY()
	TObjectPtr<const URSHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<const URSCombatSet> CombatSet;

	UPROPERTY()
	TObjectPtr<const URSEconomySet> EconomySet;

	UPROPERTY()
	TObjectPtr<const URSHandlingSet> HandlingSet;
};
