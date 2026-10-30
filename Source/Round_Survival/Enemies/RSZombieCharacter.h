// Round Survival

#pragma once

#include "AbilitySystem/RSAbilitySet.h"
#include "AbilitySystemInterface.h"
#include "Enemies/RSEnemyTypes.h"
#include "Performance/RSSignificanceSettings.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"

#include "RSZombieCharacter.generated.h"

class ARSBarricade;
class ARSZombieCharacter;
class UMotionWarpingComponent;
class URSAbilitySystemComponent;
class URSDismembermentComponent;
class URSCombatSet;
class URSEnemyDefinition;
class URSHealthSet;
struct FGameplayEffectSpec;

/**
 * Fired on the server when an enemy dies.
 *
 * @param Zombie  The enemy that died.
 * @param Killer     Player state credited with the kill, or null if nothing was.
 * @param bHeadshot  The killing shot landed on the head. Never true of a melee kill.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FRSZombieDiedEvent, ARSZombieCharacter* /*Zombie*/, AActor* /*Killer*/, bool /*bHeadshot*/);

/** Fired on the server when an enemy goes back to the pool, dead or alive: it has left the world without being destroyed. */
DECLARE_MULTICAST_DELEGATE_OneParam(FRSZombieReturnedToPoolEvent, ARSZombieCharacter* /*Zombie*/);

/**
 * What a client needs to know of the pool, replicated as one value so the two always arrive together.
 */
USTRUCT()
struct FRSEnemyPoolState
{
	GENERATED_BODY()

	/**
	 * Counts the times the body has left the pool. A body pooled and handed out again between two
	 * updates never shows a client `bInPool`, and the count is how that client still knows it is
	 * looking at a new enemy and has a ragdoll to undo. Wraps; only a change is read.
	 */
	UPROPERTY()
	uint8 Life = 0;

	UPROPERTY()
	bool bInPool = false;
};

/**
 * The enemy pawn.
 *
 * Unlike the player, it hosts its own ability system component. An enemy has no player state to
 * put one on, and nothing about it needs to outlive the pawn: when it dies, its health and buffs
 * go with it.
 *
 * Everything here is shaped by the 64-enemy budget. The component replicates in Minimal mode, so
 * clients receive attribute values and cues but never a per-effect list; only the two attribute
 * sets an enemy actually reads are created; and the mesh runs with update rate optimisation so
 * distant enemies animate at a fraction of the frame rate.
 *
 * The AI lives in the controller, on the server only. This class holds no behaviour.
 *
 * **Pooled, not destroyed.** A body that has finished dying, or that a forced round clears, goes
 * back to the pool (URSEnemyPool, through URSSpawnSubsystem) and is handed out again later as a
 * new enemy. `ReturnToPool` takes it out of the world: hidden, without collision or tick, its kit
 * taken back and every effect removed, still dead. `LeavePool` brings it back as it was spawned:
 * the living body rebuilt from the class defaults, the kit granted again on possession. The AI
 * controller stays with its body through all of it, unpossessed while pooled.
 *
 * Clients pool too. The pool state replicates, and each client puts its own copy away and brings
 * it back from that (OnRep_PoolState). A pooled body's channel goes dormant, so it costs no
 * replication while it waits, and stays open, so no client destroys its copy only to spawn it
 * again when the body comes back.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSZombieCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARSZombieCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	UFUNCTION(BlueprintCallable, Category = "RS|Enemy")
	URSAbilitySystemComponent* GetRSAbilitySystemComponent() const { return AbilitySystemComponent; }

	const URSHealthSet* GetHealthSet() const { return HealthSet; }
	const URSCombatSet* GetCombatSet() const { return CombatSet; }

	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	bool IsDead() const { return bDead; }

	/**
	 * Kills every living enemy in the world through the damage pipeline, crediting nobody: no scrap,
	 * and no power-up drop, which only a player's kill rolls for. Server only. Returns how many.
	 *
	 * What the debug wipe and the Purge power-up both are.
	 */
	static int32 KillAllUncredited(UWorld* World);

	/** Current health. A read-through to the health set, for the HUD, the debug tools and tests. */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	float GetHealth() const;

	/** Health this enemy spawned with, which the round director scales per round. */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	const URSEnemyDefinition* GetEnemyDefinition() const { return EnemyDefinition; }

	UFUNCTION(BlueprintPure, Category = "RS|Movement")
	ERSMoveTier GetMoveTier() const { return MoveTier; }

	/**
	 * Changes how fast this enemy moves. Server only; the round director calls it at spawn.
	 *
	 * The tier is the base speed, so this is not a buff and does not stack with one: a slow applied
	 * while a runner is running still divides whatever the tier below asked for.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Movement")
	void SetMoveTier(ERSMoveTier NewTier);

	/**
	 * The window this enemy came in through, or null when it simply walked in.
	 *
	 * Handed over by the spawn point that released it, which is the whole of how an enemy knows
	 * its way in. Nothing searches for a barricade and nothing infers one from a failed path: one
	 * spawn sits behind one window, so the answer is known before the enemy exists.
	 *
	 * Cleared once the window has been crossed — a spent entry, not a standing attachment — so an
	 * enemy already inside never walks back out to a window it has finished with.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	ARSBarricade* GetEntryBarricade() const;

	/** What has come off this enemy, and what decides it. */
	URSDismembermentComponent* GetDismemberment() const { return Dismemberment; }

	/** Where the vault ability puts its `FrontLedge` and `BackLedge` targets. */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	UMotionWarpingComponent* GetMotionWarping() const { return MotionWarping; }

	/** Server only. The spawn point calls it at release; the traverse state clears it. */
	UFUNCTION(BlueprintCallable, Category = "RS|Enemy")
	void SetEntryBarricade(ARSBarricade* NewBarricade);

	/**
	 * Server-side death notification, for the round director and anything else counting bodies.
	 *
	 * A delegate on the class rather than a search for listeners: the director subscribes to each
	 * enemy it spawns, and nothing has to walk the world to find out that one died.
	 */
	FRSZombieDiedEvent OnZombieDied;

	/** Server-side notification that this enemy has gone back to the pool, for whatever was counting it among the living. */
	FRSZombieReturnedToPoolEvent OnReturnedToPool;

	/**
	 * Takes this enemy out of the world and leaves it ready to be handed out again. Server only;
	 * the spawn subsystem calls it, and puts the body on the pool after. Works on a corpse and on
	 * a living enemy alike: a living one stops where it is, as if it had died, without the death.
	 */
	void ReturnToPool();

	/**
	 * Brings a pooled body back into the world at a transform, as a new enemy: alive, possessed,
	 * its kit granted. The round's health and speed are the caller's, as for a spawned one.
	 */
	void LeavePool(const FTransform& SpawnTransform);

	/** Between ReturnToPool and LeavePool, on every machine. */
	bool IsInPool() const { return PoolState.bInPool; }

	/** How much this enemy matters to the players, as the server's significance manager last decided. Server only. */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	ERSSignificanceTier GetSignificanceTier() const { return SignificanceTier; }

	/**
	 * Spends to match a significance tier. Server only, called by URSSignificanceManager; does nothing when the tier
	 * has not changed. High is the class's own settings, read from its defaults; Low keeps only the mesh's montages
	 * (URSSignificanceSettings).
	 */
	void SetSignificanceTier(ERSSignificanceTier NewTier);

protected:
	//~AActor interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	//~End of AActor interface

	//~APawn interface
	virtual void PossessedBy(AController* NewController) override;
	virtual void Restart() override;
	//~End of APawn interface

	//~ACharacter interface
	virtual bool CanCrouch() const override;
	//~End of ACharacter interface

	/** Grants the definition's ability sets. Server only, and only once per life of the pawn. */
	void GrantEnemyAbilitySets();

	UFUNCTION()
	void OnRep_MoveTier();

	/** A client's copy following the server's into the pool and out of it. */
	UFUNCTION()
	void OnRep_PoolState();

	/**
	 * Writes the tier and the multiplier into the movement component.
	 *
	 * Runs on every machine. Simulated proxies are moved by replication rather than by their own
	 * movement component, but the value also feeds the animation's speed axis, and a client that
	 * disagreed about how fast this enemy can move would blend its legs against the wrong ceiling.
	 */
	void RefreshMovementSpeed();

	/** Bound to the combat set, so a slow or a buff lands on movement without anything polling it. */
	void HandleMoveSpeedMultiplierChanged(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
		float Magnitude, float OldValue, float NewValue);

	/** Awards hit scrap. Bound to the health set on the server. */
	void HandleHealthChanged(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
		float Magnitude, float OldValue, float NewValue);

	/**
	 * Runs the death. Bound to the health set, so it runs on the server and on every client:
	 * health is replicated, and a client that only becomes relevant to this enemy after it died
	 * receives zero health and arrives at the same place.
	 */
	void HandleOutOfHealth(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
		float Magnitude, float OldValue, float NewValue);

	/** Cosmetic half of dying: ragdoll, and a corpse that stops nothing. Runs everywhere. */
	void StartRagdoll();

	/**
	 * The ragdoll undone: physics off, the mesh back on the capsule where the class puts it, the
	 * collision profiles the class gives a living enemy, and whatever montage the death interrupted
	 * stopped. Read from the class defaults rather than written out again here, so a change to the
	 * Blueprint is a change to this.
	 */
	void RestoreLivingBody();

	/** Out of a crawl, at once. Server only; clients follow the replicated `bIsCrouched`. */
	void StandUp();

	/** The body put away: hidden, no physics, no collision, no tick. Runs on every machine. */
	void StowBody();

	/** The body brought back: the living body restored, shown, colliding and ticking. Runs on every machine. */
	void UnstowBody();

	/**
	 * Lets go of the AI controller and keeps it, rather than destroying it with the body's death.
	 * Possession is where the brain starts and stops, so a pooled body is handed back its own.
	 */
	void ReleaseController();

	/**
	 * Takes the corpse out of the world, the definition's CorpseLifetime after death. Server only.
	 *
	 * Goes through `URSSpawnSubsystem::ReleaseEnemy`, the same door every enemy enters the world
	 * by, which decides whether the body goes back to the pool or is destroyed.
	 */
	virtual void FinishDeath();

	/** Applies the definition's ScrapAwardEffect to the killer's ability system. Server only. */
	void AwardScrap(AActor* RewardedActor, const FGameplayTag& RewardTag);

	/** Reward tag for what just happened, so the amount is a data lookup rather than a branch. */
	FGameplayTag SelectRewardTag(const FGameplayEffectSpec* Spec, bool bKilling) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Enemy", meta = (AllowPrivateAccess = true))
	TObjectPtr<URSAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * Bends the traversal montage's root motion onto the window actually being crossed.
	 *
	 * The clip was authored over a one-metre obstacle; a window is whatever the level says it is.
	 * Warping is what lets one animation serve every window rather than one clip per opening —
	 * and it is why the barricade publishes two points instead of one, so the climb up onto the
	 * sill and the drop down the far side are corrected separately.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Enemy", meta = (AllowPrivateAccess = true))
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	/** Charges hits to parts and takes them off; the rules are the definition's dismemberment profile. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Enemy", meta = (AllowPrivateAccess = true))
	TObjectPtr<URSDismembermentComponent> Dismemberment;

	/**
	 * What this enemy is: kit, tier speeds, rewards, corpse lifetime.
	 *
	 * Everything a designer tunes lives in the definition; this Blueprint keeps only the wiring
	 * around it. Read at possession, at death and whenever the speed is refreshed, never per frame.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Enemy")
	TObjectPtr<const URSEnemyDefinition> EnemyDefinition;

	/**
	 * Which entry of the definition's `TierSpeeds` this enemy is on. The round director sets it at
	 * spawn; the value on the Blueprint is what a hand-placed test enemy uses.
	 *
	 * Replicated because a client has to blend the same legs the server is moving.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_MoveTier, Category = "RS|Movement")
	ERSMoveTier MoveTier = ERSMoveTier::Walk;

private:
	/** What the grant handed out, so EndPlay and the pool can take it all back. */
	UPROPERTY()
	FRSAbilitySet_GrantedHandles EnemyAbilityHandles;

	/**
	 * Set once the kit is granted, and cleared when the pool takes it back. Possession can happen
	 * more than once in one life — a controller swap — and granting twice would apply the initial
	 * attribute effect twice and stack every passive.
	 */
	bool bEnemyAbilitySetsGranted = false;

	/** Set on whichever machine noticed the death first, so the death path runs once per life. */
	bool bDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_PoolState)
	FRSEnemyPoolState PoolState;

	/** A client's record of the life it last applied. */
	uint8 AppliedPoolLife = 0;

	/** The tier this enemy is spending at. High until the significance manager says otherwise, and again whenever it lets go. */
	ERSSignificanceTier SignificanceTier = ERSSignificanceTier::High;

	/**
	 * The AI controller this body let go of when it died or was pooled, to possess it again when
	 * it leaves the pool. Destroyed with the body if the body is destroyed instead.
	 */
	UPROPERTY(Transient)
	TObjectPtr<AController> KeptController;

	/**
	 * The window it entered by. Server only and weak: a barricade is level geometry that outlives
	 * any one enemy, and nothing here should keep one alive.
	 */
	TWeakObjectPtr<ARSBarricade> EntryBarricade;

	FTimerHandle CorpseTimerHandle;

	/**
	 * Health and combat only. An enemy never holds scrap and never reloads, and every attribute
	 * set replicates on its own regardless of the component's replication mode, so any other set
	 * here would be sent 64 times over to be read by nobody.
	 */
	UPROPERTY()
	TObjectPtr<const URSHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<const URSCombatSet> CombatSet;
};
