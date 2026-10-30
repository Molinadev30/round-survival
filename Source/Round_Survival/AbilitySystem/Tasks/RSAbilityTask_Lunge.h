// Round Survival

#pragma once

#include "Abilities/Tasks/AbilityTask_ApplyRootMotion_Base.h"

#include "RSAbilityTask_Lunge.generated.h"

/**
 * Runs the melee lunge (FRSRootMotionSource_Lunge) towards one enemy for a set time.
 *
 * Applies the source to the avatar's movement component, keeps its target where the enemy is every
 * frame, and takes it off when the time runs out, when the player is StopDistance from the enemy, or
 * when the enemy is gone. Created by the ability on the striker and on the server, each with its own
 * copy of the enemy, which is how the engine's own move-to-actor force works too.
 *
 * Not a simulated task: everybody else's copy of the player gets the source itself, through the
 * character's replicated root motion, and has nothing to run.
 */
UCLASS()
class ROUND_SURVIVAL_API URSAbilityTask_Lunge : public UAbilityTask_ApplyRootMotion_Base
{
	GENERATED_BODY()

public:
	URSAbilityTask_Lunge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * @param Duration          Seconds the lunge lasts: until the blow lands.
	 * @param StopDistance      How far short of the enemy it stops, centre to centre, in the plane.
	 * @param MaxSpeed          The fastest it moves the player.
	 * @param FinishMaxSpeed    The horizontal speed the player is held to when it ends, however it ends. A fall keeps falling.
	 */
	static URSAbilityTask_Lunge* Lunge(UGameplayAbility* OwningAbility, AActor* Target, float Duration, float StopDistance, float MaxSpeed,
		float FinishMaxSpeed);

	//~UGameplayTask interface
	virtual void TickTask(float DeltaTime) override;
	//~End of UGameplayTask interface

protected:
	//~UAbilityTask_ApplyRootMotion_Base interface
	virtual void SharedInitAndApply() override;
	//~End of UAbilityTask_ApplyRootMotion_Base interface

	//~UGameplayTask interface
	virtual void OnDestroy(bool bInOwnerFinished) override;
	//~End of UGameplayTask interface

private:
	/** Puts the enemy's location and velocity on the source. False once the enemy is gone. */
	bool UpdateTarget();

	TWeakObjectPtr<AActor> Target;
	float Duration = 0.0f;
	float StopDistance = 0.0f;
	float MaxSpeed = 0.0f;
};
