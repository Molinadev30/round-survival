// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_Interact.generated.h"

class AActor;
class URSInteractorComponent;

/**
 * Using whatever the pawn is looking at.
 *
 * Interaction is an ability rather than a function on the pawn because everything a hold needs
 * already exists in the ability system: `RS.State.Interacting` for as long as it runs, so
 * animation and other abilities read one state; cancellation, so looking away or dying stops it;
 * and a cost hook, which the purchasables of 7.2 charge scrap through.
 *
 * **The server never takes the client's word for what was used.** Both sides run the ability, and
 * each reads the target from its own interactor component — the server's own trace, from the
 * replicated control rotation. `Interact` is then called behind an authority check. A client that
 * claims to be looking at a locker it is not looking at simply has its hold cancelled by the
 * server, and nothing is bought.
 *
 * **One press is one interaction, unless the target says otherwise.** A repeatable target — a
 * barricade — runs another hold as soon as one finishes, which is what lets a single hold put
 * board after board back and pay for each one as it lands. A non-repeatable one keeps the ability
 * alive doing nothing until the key is released, which is what stops a held key buying twice.
 */
UCLASS()
class ROUND_SURVIVAL_API URSGameplayAbility_Interact : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End of UGameplayAbility interface

	/**
	 * What this activation is holding, or null between steps.
	 *
	 * Read by `URSAbilityCost_Scrap`: once the hold is under way this is the validated target,
	 * and therefore the only thing whose price may legitimately be charged.
	 */
	AActor* GetCurrentInteractionTarget() const { return CurrentTarget.Get(); }

protected:
	/** The interactor component on this pawn, or null if the avatar has none. */
	UFUNCTION(BlueprintPure, Category = "RS|Interaction")
	URSInteractorComponent* GetInteractorComponent() const;

	/** Reads the current focus and starts one hold on it. Ends the ability when there is nothing. */
	void BeginInteractionStep();

	/** The hold finished: interact on the server, then either loop or wait out the key. */
	void CompleteInteractionStep();

	UFUNCTION()
	void OnHoldProgress(float Progress);

	UFUNCTION()
	void OnHoldCompleted();

	UFUNCTION()
	void OnHoldCancelled();

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** Tells the target its hold is over, if one was announced, and gives the weapon hands back. */
	void EndHold();

	/** The target takes the weapon hands: stops a burst or reload, and holds RS.State.HandsBusy. */
	void TakeHands();

	/** Takes RS.State.HandsBusy off again, if this activation put it on. */
	void ReleaseHands();

	/** Cancelled when a target takes the weapon hands. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Interaction")
	FGameplayTagContainer WeaponAbilitiesToInterrupt;

private:
	/** What this activation is holding. Re-read from the component at the start of every step. */
	TWeakObjectPtr<AActor> CurrentTarget;

	/** The target was told a hold started and has not yet been told it ended. Server only. */
	TWeakObjectPtr<AActor> HeldTarget;

	/** True once a non-repeatable interaction is done and the ability is only waiting for the key. */
	bool bWaitingForRelease = false;

	/** This activation put RS.State.HandsBusy on and has not taken it off yet. */
	bool bHandsTaken = false;
};
