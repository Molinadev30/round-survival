// Round Survival

#pragma once

#include "Equipment/RSGameplayAbility_FromEquipment.h"

#include "RSGameplayAbility_RangedWeapon.generated.h"

class APawn;
class UGameplayEffect;
class URSRangedWeaponInstance;
struct FCollisionQueryParams;
struct FGameplayAbilityTargetDataHandle;

/**
 * Firing a ranged weapon.
 *
 * The shot is resolved in three steps, which exists to stop a player shooting through a corner
 * the camera can see past but the barrel cannot:
 *
 *   1. Aim segment    camera -> AimPoint, along the control rotation. Establishes where the
 *                     reticle points. Deals nothing.
 *   2. Muzzle resolve MuzzleOrigin = camera + a camera-relative offset. A short trace from the
 *                     camera pulls it back if it would start inside geometry.
 *   3. Bullet segment MuzzleOrigin -> AimPoint, with spread applied here, at the muzzle. This is
 *                     the shot, and anything in the way stops it.
 *
 * The origin is camera-relative and never a mesh socket: the viewmodel is not rendered where its
 * components are, it exists only on the owning client, and third-person sockets depend on
 * animation the server evaluates differently. A camera-relative offset is the only origin every
 * machine can reproduce, which is what makes server validation possible at all.
 *
 * **One activation is one trigger pull; the cadence is a clock inside it.** A semi-automatic weapon
 * fires one shot per activation. An automatic one stays active while the trigger is held and fires
 * every shot that has come due, by world time, at the weapon's rounds per minute. Shots are due at
 * exact multiples of the interval from the first one, never from when the last one actually went
 * out, so the cadence does not depend on the frame rate: a frame that arrives late fires the late
 * shot, and a frame longer than the interval fires two. A timer only wakes the ability for the next
 * due shot; it decides nothing. The weapon instance remembers when the next shot is due, so a pull
 * that comes too soon waits instead of firing early.
 *
 * Every shot is its own prediction window and its own target data, so the server validates, charges
 * and applies each one on its own; the activation and the end are sent once per pull. The server
 * additionally refuses shots its per-weapon budget cannot cover.
 */
UCLASS()
class ROUND_SURVIVAL_API URSGameplayAbility_RangedWeapon : public URSGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:
	URSGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	URSRangedWeaponInstance* GetWeaponInstance() const;

	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End of UGameplayAbility interface

	//~URSGameplayAbility interface
	/** True for an automatic weapon: holding the trigger keeps firing at the weapon's cadence. */
	virtual bool ShouldActivateWhileInputHeld(const FGameplayAbilitySpec& Spec, const FGameplayAbilityActorInfo* ActorInfo) const override;
	//~End of URSGameplayAbility interface

protected:
	/** The resolved geometry of one trigger pull, on whichever machine computed it. */
	struct FRSFiringSolution
	{
		/** Where the shot starts. Camera plus the weapon offset, pulled back out of geometry. */
		FVector MuzzleOrigin = FVector::ZeroVector;

		/** Where the reticle is pointing, as far as the aim trace got. */
		FVector AimPoint = FVector::ZeroVector;

		/** Camera forward, used for the aim segment and as the spread cone's axis. */
		FVector AimDirection = FVector::ForwardVector;

		/** True when the target is closer than the muzzle and the bullet segment collapsed. */
		bool bCollapsedToAimSegment = false;

		URSRangedWeaponInstance* Weapon = nullptr;
	};

	/** Runs steps 1 and 2. Deterministic given a view point, so the server reproduces it. */
	bool BuildFiringSolution(OUT FRSFiringSolution& OutSolution) const;

	/** Runs step 3 once per bullet in the cartridge, applying spread at the muzzle. */
	void TraceBulletsInCartridge(const FRSFiringSolution& Solution, OUT TArray<FHitResult>& OutHits) const;

	FHitResult WeaponTrace(const FVector& Start, const FVector& End, float SweepRadius, OUT TArray<FHitResult>& OutHits) const;

	virtual void AddAdditionalTraceIgnoreActors(FCollisionQueryParams& TraceParams) const;

	/** Fires one shot: builds the solution, traces, and hands the result to OnTargetDataReadyCallback. */
	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	void StartRangedWeaponTargeting();

	/**
	 * Re-runs the shot server-side and rejects impacts its own trace does not agree with.
	 *
	 * A client reporting a hit through a wall, at an impossible range, or on an actor the server's
	 * bullet segment never reached has that hit replaced by the server's own result.
	 *
	 * @param OutServerMuzzleOrigin  Where the server's shot started, or unset if the server could not
	 *                               build a firing solution at all. Damage falloff is measured from
	 *                               here, never from the trace start the client reported.
	 */
	bool ValidateTargetData(OUT FGameplayAbilityTargetDataHandle& TargetData, OUT TOptional<FVector>& OutServerMuzzleOrigin) const;

	/**
	 * Applies DamageEffect to every actor the validated shot hit. Server only.
	 *
	 * Unpredicted on purpose: a client that subtracted health locally would show an enemy hurt by
	 * a shot the server then rejected, and for enemies there is nothing to gain from predicting it.
	 */
	void ApplyDamageToTargets(const FGameplayAbilityTargetDataHandle& TargetData, const FVector& MuzzleOrigin);

	/** Hook for cues, effects and recoil once the shot is confirmed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "RS|Weapon", meta = (DisplayName = "OnRangedWeaponTargetDataReady"))
	void K2_OnRangedWeaponTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	/**
	 * Muzzle position relative to the camera, in camera space: X forward, Y right, Z up.
	 *
	 * Tuned to sit roughly where the viewmodel's barrel appears on screen. It is a cosmetic match
	 * only — what matters is that both machines compute the same number.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ForceUnits = cm))
	FVector MuzzleOffsetFromCamera = FVector(30.0f, 12.0f, -10.0f);

	/** How far behind the blocking surface the origin is placed when the muzzle is buried. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ForceUnits = cm))
	float MuzzlePullbackPadding = 2.0f;

	/**
	 * The effect applied per bullet that hits something with an ability system.
	 *
	 * Expected to run URSDamageExecution. The weapon's base damage is set on it as
	 * RS.SetByCaller.Damage, so one effect serves every weapon.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** How far a client's reported impact may sit from the server's before it is thrown out. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ForceUnits = cm))
	float ServerValidationTolerance = 100.0f;

	/**
	 * How far past a client's claimed impact the server's verification trace continues.
	 *
	 * Long enough to register the surface the claim sits on, short enough that a wall behind the
	 * target is rarely inside it. The server reads the hit zone from what this trace touches.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ForceUnits = cm, ClampMin = 0.0))
	float HitVerificationTraceExtension = 10.0f;

private:
	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	/**
	 * Shooter only. Fires every shot that is due by now, then either ends the pull or sets the timer
	 * for the next one. Called on activation and by that timer.
	 */
	void FireDueShots();

	/** Server: refuses a shot the weapon's budget cannot cover. The client's hit markers for it are dropped. */
	void RejectShot(const FGameplayAbilityTargetDataHandle& TargetData);

	/** Wakes FireDueShots for the next due shot. */
	FTimerHandle ShotTimerHandle;

	/** Shooter only: the trigger is still down. An automatic weapon keeps firing while it is. */
	bool bTriggerHeld = false;

	/** Shooter only: this pull has fired at least once. Until it has, letting go does not end it. */
	bool bHasFiredThisPull = false;

	FDelegateHandle TargetDataReadyDelegateHandle;
};
