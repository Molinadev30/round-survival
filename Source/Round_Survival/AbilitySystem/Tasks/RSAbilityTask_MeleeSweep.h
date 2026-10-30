// Round Survival

#pragma once

#include "Abilities/Tasks/AbilityTask.h"

#include "RSAbilityTask_MeleeSweep.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSMeleeSweepHitDelegate, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSMeleeSweepMissedDelegate);

/**
 * Looks for something to hit, every frame, for as long as a swing's damage window is open.
 *
 * Three filters, cheapest first, and all three have to pass:
 *
 * 1. **A swept sphere**, not a line. A thin ray through the middle of a swing misses a player
 *    who is standing slightly off to the side of an animation that visibly reached them.
 * 2. **A frontal cone.** Getting behind the attacker after it committed to the swing is a real
 *    dodge, so the direction to the target is checked against where the attacker is facing.
 * 3. **A line of sight trace.** The sphere goes through walls; this is what stops a swing landing
 *    through one. The ignore list is how an attacker reaching through a barricade it is holding
 *    still connects: that one actor is excluded, every other wall is not.
 *
 * The first target that passes ends the task. One swing, one victim.
 */
UCLASS()
class ROUND_SURVIVAL_API URSAbilityTask_MeleeSweep : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_MeleeSweep(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * @param Duration          How long the window stays open. Comes from the notify state.
	 * @param Range             How far in front of the attacker the swing reaches.
	 * @param SweepRadius       Radius of the swept sphere.
	 * @param HalfAngleDegrees  Half the frontal cone. 60 gives the usual 120 degree arc.
	 * @param IgnoredActors     Actors the line of sight trace does not treat as cover.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static URSAbilityTask_MeleeSweep* MeleeSweep(UGameplayAbility* OwningAbility, float Duration, float Range,
		float SweepRadius, float HalfAngleDegrees, const TArray<AActor*>& IgnoredActors, bool bDrawDebug);

	/**
	 * Sweeps from a fixed point and direction instead of from the attacker's body.
	 *
	 * For a blow that does not start where the attacker stands — an arm reaching through a gap in
	 * a barricade. Call before `ReadyForActivation`. The line of sight trace starts from the same
	 * point, so what counts as cover is judged from where the hand actually is.
	 */
	void SetFixedFrame(const FVector& InOrigin, const FVector& InForward);

	/** A target passed all three filters. */
	UPROPERTY(BlueprintAssignable)
	FRSMeleeSweepHitDelegate OnHit;

	/** The window closed with nothing in it. */
	UPROPERTY(BlueprintAssignable)
	FRSMeleeSweepMissedDelegate OnMissed;

	//~UAbilityTask interface
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	//~End of UAbilityTask interface

protected:
	/** One frame's worth of looking. Returns true and fills OutHit when something qualifies. */
	bool FindTarget(FHitResult& OutHit) const;

	/** Whether the attacker can actually see the point it is about to hit. */
	bool HasLineOfSight(const FVector& Origin, const AActor* Target) const;

	/** The swept shape and the cone it is filtered by, drawn together because they fail alike. */
	void DrawSweep(const UWorld* World, const FVector& Origin, const FVector& End, const FVector& Forward, bool bConnected) const;

	/** True when this ability asked for debug drawing, or when `RS.Enemy.Debug` is on. */
	bool ShouldDraw() const;

	float Duration = 0.0f;
	float Range = 175.0f;
	float SweepRadius = 25.0f;
	float HalfAngleDegrees = 60.0f;
	float CosHalfAngle = 0.5f;
	bool bDrawDebug = false;

	bool bHasFixedFrame = false;
	FVector FixedOrigin = FVector::ZeroVector;
	FVector FixedForward = FVector::ForwardVector;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> IgnoredActors;

	float TimeRemaining = 0.0f;
};
