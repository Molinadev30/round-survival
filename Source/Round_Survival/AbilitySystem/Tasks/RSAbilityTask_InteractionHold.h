// Round Survival

#pragma once

#include "Abilities/Tasks/AbilityTask.h"

#include "RSAbilityTask_InteractionHold.generated.h"

class AActor;
class URSInteractorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSInteractionHoldProgressDelegate, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRSInteractionHoldEventDelegate);

/**
 * Holds an interaction open for its duration, and gives up the moment it stops being valid.
 *
 * It ticks rather than waiting out a single delay, for two reasons. A hold has to be *watchable*
 * — the HUD draws a filling ring, so somebody has to publish progress — and it has to end early
 * when the player looks away, walks out of range or when the target stops accepting interactions
 * mid-hold, which a delay that only reports at the end cannot do.
 *
 * The validity check costs nothing: the interactor component is already refreshing its focus on
 * its own timer, so this compares a pointer and asks the target one question. It does not trace.
 */
UCLASS()
class ROUND_SURVIVAL_API URSAbilityTask_InteractionHold : public UAbilityTask
{
	GENERATED_BODY()

public:
	URSAbilityTask_InteractionHold(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * @param Duration    Seconds the hold takes, as the target reported it.
	 * @param Target      The interactable being held. Losing it cancels the hold.
	 * @param Interactor  The pawn's interactor component, read for the current focus.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static URSAbilityTask_InteractionHold* InteractionHold(UGameplayAbility* OwningAbility, float Duration,
		AActor* Target, URSInteractorComponent* Interactor);

	/** How far along the hold is, 0 to 1, once per frame. */
	UPROPERTY(BlueprintAssignable)
	FRSInteractionHoldProgressDelegate OnProgress;

	/** The hold ran its full duration. */
	UPROPERTY(BlueprintAssignable)
	FRSInteractionHoldEventDelegate OnCompleted;

	/** The target was lost, moved out of focus, or stopped accepting the interaction. */
	UPROPERTY(BlueprintAssignable)
	FRSInteractionHoldEventDelegate OnCancelled;

	//~UAbilityTask interface
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	//~End of UAbilityTask interface

protected:
	/** Still looking at the same target, and it still accepts the interaction. */
	bool IsStillValid() const;

	float Duration = 0.0f;
	float Elapsed = 0.0f;

	UPROPERTY()
	TWeakObjectPtr<AActor> Target;

	UPROPERTY()
	TWeakObjectPtr<URSInteractorComponent> Interactor;
};
