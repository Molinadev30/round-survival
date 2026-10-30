// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_Melee.generated.h"

class UAnimMontage;
class UGameplayEffect;
struct FGameplayEventData;

/**
 * One melee swing.
 *
 * Written for the enemy but not tied to it: it plays a montage, waits for that montage to say its
 * damage window has opened, looks for a target while the window lasts, and applies a damage effect
 * to the first one it finds. Anything that wants a swing of this kind configures a subclass with its
 * own montage, reach and damage. The player's strike is not one: it is predicted, timed by data
 * rather than by its animation, and aimed by the view (URSGameplayAbility_PlayerMelee).
 *
 * **The montage is the cadence.** The ability lives as long as the montage does, so an attacker
 * that swings and misses is committed to the rest of the animation before it can try again. That
 * is a deliberate window for the player to step back into and walk away through, and it is why
 * there is no separate cooldown effect: two numbers governing one rhythm would drift apart.
 *
 * Server only and unpredicted, in line with the enemy budget: nothing on a client needs to guess
 * the outcome of a swing, and sixty-four predicted abilities would cost more than they buy.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_Melee : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Melee(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UGameplayAbility interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End of UGameplayAbility interface

	/**
	 * Actors the line of sight trace must not treat as cover.
	 *
	 * Empty here. URSGameplayAbility_ZombieMelee overrides it with the barricade this attacker has a slot on, so an enemy
	 * at a window can reach through the boards it is holding without being able to swing through
	 * any other wall in the level.
	 */
	virtual void GetLineOfSightExceptions(TArray<AActor*>& OutActors) const;

	/**
	 * Where the swing starts and which way it goes, when that is not the attacker's own body.
	 *
	 * False here: the sweep follows the attacker, which is right for a swing thrown from where it
	 * stands. A blow thrown from somewhere else — through a gap in a barricade — overrides this.
	 */
	virtual bool GetSweepFrame(FVector& OutOrigin, FVector& OutForward) const;

	/** Damage window opened. Starts looking for a target. */
	UFUNCTION()
	void HandleWindowOpened(FGameplayEventData Payload);

	/** Something qualified. Applies the damage effect to it. */
	UFUNCTION()
	void HandleSweepHit(const FHitResult& Hit);

	/** The window closed empty. Kept separate from a hit so it can be logged and, later, voiced. */
	UFUNCTION()
	void HandleSweepMissed();

	UFUNCTION()
	void HandleMontageFinished();

	/** The montage this activation plays. `AttackMontage` unless a subclass picks per swing. */
	virtual UAnimMontage* GetAttackMontage() const { return AttackMontage; }

	/** The swing. Its `RS Melee Window` notify state is what decides when the blow lands. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Expected to carry `RS.Damage.Type.Melee` and read `RS.SetByCaller.Damage`. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ClampMin = 0.0))
	float Damage = 40.0f;

	/** How far in front the swing reaches, measured from capsule centre to capsule centre. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ClampMin = 0.0, ForceUnits = cm))
	float Range = 175.0f;

	/** Radius of the swept sphere, so a target slightly off the centre line still counts. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ClampMin = 1.0, ForceUnits = cm))
	float SweepRadius = 30.0f;

	/** Half the frontal cone. 60 gives a 120 degree arc; getting behind the swing dodges it. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ClampMin = 0.0, ClampMax = 180.0, ForceUnits = deg))
	float ConeHalfAngle = 60.0f;

	/** Draws the sweep and the line of sight trace. For tuning reach against the animation. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee")
	bool bDrawDebug = false;

	/** Playback rate of the montage, which is also how fast the attacker can swing again. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ClampMin = 0.01))
	float MontageRate = 1.0f;

	/**
	 * How much each swing's playback rate strays from `MontageRate`, either way: 0.1 plays each one
	 * somewhere between 0.9 and 1.1 times as fast. Attackers standing side by side drift out of
	 * step instead of swinging in unison, and the average rhythm stays where `MontageRate` puts it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee", meta = (ClampMin = 0.0, ClampMax = 0.5))
	float MontageRateVariance = 0.0f;
};
