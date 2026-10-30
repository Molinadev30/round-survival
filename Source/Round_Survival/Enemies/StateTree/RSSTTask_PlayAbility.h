// Round Survival

#pragma once

#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Tasks/StateTreeAITask.h"

#include "RSSTTask_PlayAbility.generated.h"

class AActor;

USTRUCT()
struct FRSSTTask_PlayAbilityInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the controlled pawn, which is where an enemy's ability system lives. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	/** Which ability to run. Matched against the ability's own asset tags. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag AbilityTag;

	/**
	 * Waits a random time up to this long before activating, when starting from rest. Enemies that
	 * reach a window together would otherwise swing in the same frame. A swing that follows
	 * straight on from the previous one does not wait, so the body never drops back to idle
	 * between blows.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = s))
	float MaxStartDelay = 0.0f;

	/** What is left of this entry's wait before activating. */
	UPROPERTY()
	float RemainingDelay = 0.0f;

	/** The activation this task is waiting on. */
	UPROPERTY()
	FGameplayAbilitySpecHandle ActivatedHandle;
};

/**
 * Runs one gameplay ability and holds the state open until it finishes.
 *
 * The bridge between the two halves of the enemy: the tree decides *when* to attack, and the
 * ability owns *what attacking is* — its animation, its damage window, its reach. Neither knows
 * the other's business, so the melee ability is equally usable from the state that stands in
 * front of a player and from the one that tears at a barricade.
 *
 * The state ends when the ability does, which makes the animation's own length the attack's
 * cadence. Leaving the state early cancels the ability rather than letting it play on underneath
 * whatever replaced it.
 */
USTRUCT(meta = (DisplayName = "Play Ability", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTTask_PlayAbility : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTTask_PlayAbilityInstanceData;

	FRSSTTask_PlayAbility();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	/** Activates the ability now. Running if it started, Failed if nothing would. */
	EStateTreeRunStatus Activate(FStateTreeExecutionContext& Context) const;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
#endif // WITH_EDITOR
};
