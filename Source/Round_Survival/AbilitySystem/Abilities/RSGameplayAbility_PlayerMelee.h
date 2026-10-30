// Round Survival

#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_PlayerMelee.generated.h"

class UGameplayEffect;
class URSMeleeStrikeSet;
struct FGameplayAbilityTargetDataHandle;

/**
 * What the striker decided on the press, sent to the server so both run the same strike: which of the
 * weapon's strikes it is (INDEX_NONE for the lunge), and the enemy it lunges at, if any.
 */
USTRUCT()
struct ROUND_SURVIVAL_API FRSGameplayAbilityTargetData_MeleePlan : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Variant = 0;

	UPROPERTY()
	TWeakObjectPtr<AActor> LungeTarget;

	virtual TArray<TWeakObjectPtr<AActor>> GetActors() const override;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FRSGameplayAbilityTargetData_MeleePlan::StaticStruct();
	}
};

template<>
struct TStructOpsTypeTraits<FRSGameplayAbilityTargetData_MeleePlan> : public TStructOpsTypeTraitsBase2<FRSGameplayAbilityTargetData_MeleePlan>
{
	enum
	{
		WithNetSerializer = true
	};
};

/**
 * The player's melee strike: one press, one blow, wherever the view points.
 *
 * **Timed by the animation it plays, as data.** The drawn weapon names its strikes (URSMeleeStrikeSet):
 * montages, and the moments in them the blow lands and the hands come back, divided by the set's play
 * rate. The blow lands on the frame the animation shows it landing, whichever animation it is. A
 * weapon with no strikes uses ImpactDelay and StrikeDuration here. This is the opposite of the enemy's
 * swing (URSGameplayAbility_Melee), whose montage is its cadence and decides nothing else.
 *
 * **What it hits.** Rays from the view point, cast in a fixed order: straight ahead first, then a fan
 * that opens around it to the four sides and the four corners of a rectangle FanHalfExtent across at
 * full reach. The first ray whose first blocking hit is a living enemy is the blow; a ray stopped by
 * a wall or a teammate is no blow, and the next ray is tried. Each ray starts OriginPullback behind
 * the eye, so an enemy pressed right up against the player is still in front of where the ray begins.
 * The fan is in the view's own frame with its roll left out, so the slide's tilt does not tilt it.
 *
 * **The lunge.** On the press the same fan is cast farther — LungeReach on the ground, LungeReachInAir
 * in the air — and an enemy it lands on more than LungeStopDistance away, that the player is already
 * moving towards at LungeMinApproachSpeed or more, is lunged at: carried towards
 * until the blow lands (FRSRootMotionSource_Lunge), and struck at that same reach. The lunge's blow is
 * aimed at the enemy it lunged at, wherever the view has turned meanwhile: a single ray from the eye
 * to the middle of its body. Only if that ray does not land — the enemy dead, out of reach, or behind
 * a wall or a teammate — is the fan cast from the view as for any blow. A lunge plays the set's lunge
 * strike; otherwise the strikes alternate.
 *
 * **Predicted on the striker, decided on the server.** On the press the striker decides the strike —
 * which one, and whom it lunges at — runs it, and sends it as a plan (FRSGameplayAbilityTargetData_MeleePlan).
 * The server checks the lunge as it checks a blow and runs the same strike, lunging only at an enemy
 * it agrees on; one it refuses is not run there, and the movement component corrects the striker back.
 * When the blow lands the striker casts the rays on its own machine and sends what it hit. The server
 * does not recast them — the enemy has moved by the time the claim arrives — but checks the claim: a
 * living enemy, at a point no farther from the server's own view point than the fan reaches, plus a
 * tolerance, on that enemy's bounds, with no wall in between. Only then is the damage applied, on the
 * server alone.
 *
 * **On screen,** through the weapon cues: GameplayCue.Weapon.Melee is added for the strike's length,
 * so one cut short stops on every machine, and GameplayCue.Weapon.MeleeImpact is executed where the
 * blow landed. Both go on within the plan's and the blow's prediction windows, so the striker plays
 * them at once and the server's copies reach everybody else.
 *
 * Granted with the player, not the weapon. It cuts a shot, a reload, the sights and a sprint, and
 * keeps them and a weapon switch from starting until it is over; held keys bring them back after.
 * A switch, two busy hands or going down keep it from starting, and going down or a two-handed
 * interaction cancels one under way.
 */
UCLASS()
class ROUND_SURVIVAL_API URSGameplayAbility_PlayerMelee : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_PlayerMelee(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UGameplayAbility interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End of UGameplayAbility interface

protected:
	/** Seconds from the press to the blow landing, for a weapon with no strikes of its own. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ForceUnits = s, ClampMin = 0.0))
	float ImpactDelay = 0.25f;

	/** Seconds from the press until the hands are free, for a weapon with no strikes of its own. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ForceUnits = s, ClampMin = 0.0))
	float StrikeDuration = 0.75f;

	/** How far ahead of the eye the rays reach. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ForceUnits = cm, ClampMin = 0.0))
	float Reach = 200.0f;

	/** How far behind the eye the rays start. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ForceUnits = cm, ClampMin = 0.0))
	float OriginPullback = 32.0f;

	/** Half the width and half the height of the fan at full reach. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ForceUnits = cm))
	FVector2D FanHalfExtent = FVector2D(32.0f, 32.0f);

	/**
	 * Where each ray ends at full reach, as a fraction of FanHalfExtent: X across the view to the right,
	 * Y up it. Cast in this order and the first ray to land wins, so straight ahead goes first.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee")
	TArray<FVector2D> FanPattern;

	/** Expected to carry RS.Damage.Type.Melee, which is what the enemy pays its melee rewards on, and to read RS.SetByCaller.Damage. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ClampMin = 0.0))
	float Damage = 150.0f;

	/**
	 * Slack the server allows a claim, both past the fan's reach and off the enemy's bounds: the enemy
	 * kept moving while the claim was on its way.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ForceUnits = cm, ClampMin = 0.0))
	float ServerValidationTolerance = 60.0f;

	/** How far ahead of the eye an enemy can be lunged at from the ground. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee|Lunge", meta = (ForceUnits = cm, ClampMin = 0.0))
	float LungeReach = 370.0f;

	/** The same, from the air. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee|Lunge", meta = (ForceUnits = cm, ClampMin = 0.0))
	float LungeReachInAir = 204.0f;

	/** How far short of the enemy the lunge stops, centre to centre. An enemy this close already is not lunged at. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee|Lunge", meta = (ForceUnits = cm, ClampMin = 0.0))
	float LungeStopDistance = 96.0f;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee|Lunge", meta = (ForceUnits = "cm/s", ClampMin = 0.0))
	float LungeMaxSpeed = 1914.0f;

	/**
	 * How fast the player has to be moving towards the enemy, in the plane, for the strike to lunge.
	 * Standing still, backing off or strafing past it, the blow is struck from where the player is.
	 * The server asks for half, for the time its copy of the player's speed is behind.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee|Lunge", meta = (ForceUnits = "cm/s", ClampMin = 0.0))
	float LungeMinApproachSpeed = 100.0f;

private:
	/** The view the rays are cast from: the eye, and the view's rotation with its roll left out. */
	bool GetStrikeView(FVector& OutEye, FRotator& OutRotation) const;

	/** Casts the fan to InReach. True, with the hit, when a ray lands on an enemy. */
	bool FindStrikeTarget(float InReach, FHitResult& OutHit) const;

	/** Casts a single ray to InReach at the middle of Target's body, whatever the view. True, with the hit, when it lands on an enemy. */
	bool FindStrikeOn(const AActor* Target, float InReach, FHitResult& OutHit) const;

	/** A living enemy: an ability system, not a player, not dead. */
	bool IsStrikeTarget(const AActor* Actor) const;

	/** The reach a lunge starts from right now: on the ground or in the air. */
	float GetLungeReachNow() const;

	/** How fast the player is moving towards Target, in the plane: negative backing off, zero strafing past. */
	float GetApproachSpeed(const AActor* Target) const;

	/** How far the server lets a claim at InReach sit from its own eye. */
	float GetAllowedDistance(float InReach) const;

	/** The drawn weapon's strikes, or null. */
	const URSMeleeStrikeSet* GetStrikeSet() const;

	/** Seconds to the blow and to the hands coming back, for a strike of the drawn weapon or this ability's own. */
	void GetStrikeTiming(int32 Variant, float& OutImpactDelay, float& OutDuration) const;

	/** The longest any strike of the drawn weapon lasts: how long the server holds the hands before the plan arrives. */
	float GetLongestStrikeDuration() const;

	/** The striker's machine, on the press: an enemy in lunge reach far enough to lunge at, or null. */
	AActor* FindLungeTarget() const;

	/** Server: whether the enemy the striker lunged at is one it could have. */
	bool ValidateLungeTarget(const AActor* Target) const;

	/** Server: whether the striker's claim is one the server accepts. */
	bool ValidateStrike(const FHitResult& Hit) const;

	/**
	 * Runs the strike the plan names, on this machine: its timing, counted from when the strike began,
	 * and the lunge, when there is a target. On the striker as it decides it, and on the server as the plan arrives.
	 */
	void BeginStrike(int32 Variant, AActor* LungeTarget);

	/** Carries the player at Target until the blow lands. */
	void StartLunge(AActor* Target, float TimeToImpact);

	/** The striker's machine, when the blow lands: casts the fan and hands on what it found. */
	void Strike();

	/** Where the strike's target data arrives: the plan and the blow, from the striker on its own machine and through its RPC on the server. */
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	void FinishStrike();

	FTimerHandle ImpactTimerHandle;
	FTimerHandle FinishTimerHandle;
	FDelegateHandle TargetDataReadyDelegateHandle;

	/** World time the strike began, on this machine. */
	double StrikeStartTime = 0.0;

	/** The reach this strike is cast at, and on the server checked at: Reach, or the lunge's when it lunged. */
	float StrikeReach = 0.0f;

	/** The enemy this strike lunged at, which its blow is aimed at. Null for a strike that did not lunge. */
	TWeakObjectPtr<AActor> StrikeTarget;

	/** Which of the alternating strikes comes next, on the striker. */
	int32 NextStrike = 0;
};
