// Round Survival

#pragma once

#include "Components/PawnComponent.h"
#include "Internationalization/Text.h"

#include "RSInteractorComponent.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSInteractionFocusChanged, AActor*, FocusedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRSInteractionHoldProgress, float, Progress);
DECLARE_MULTICAST_DELEGATE_OneParam(FRSInteractionPromptChanged, const FText& /*Prompt*/);

/**
 * Finds what the pawn is looking at, and is the one place the HUD asks about it.
 *
 * The trace runs on a timer at `FocusRefreshInterval`, never on tick, and the first fire is
 * offset at random inside the interval so that several players do not all trace on the same
 * frame for ever.
 *
 * **It runs on the owning client and on the server, and for different reasons.** The client
 * needs a focus to draw a prompt; the server needs one because it is the authority on what was
 * interacted with, and a client-supplied target would be a claim to validate rather than a fact.
 * Both traces start from the same view point — the server has the replicated control rotation —
 * so they agree, and where they do not, the server wins and the hold is cancelled.
 *
 * Focus is "implements the interface", not "may be used right now": a locker the player cannot
 * afford still shows its prompt. `CanInteract` gates the ability, not the trace.
 */
UCLASS(BlueprintType, ClassGroup = (RS), meta = (BlueprintSpawnableComponent))
class ROUND_SURVIVAL_API URSInteractorComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URSInteractorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** What the pawn is looking at, as of the last refresh. Null when nothing qualifies. */
	UFUNCTION(BlueprintPure, Category = "RS|Interaction")
	AActor* GetFocusedInteractable() const { return FocusedActor.Get(); }

	/** The focused interactable's prompt, or empty text when there is no focus. */
	UFUNCTION(BlueprintPure, Category = "RS|Interaction")
	FText GetFocusPrompt() const;

	/** How far into the current hold this pawn is, 0 to 1. Zero when not interacting. */
	UFUNCTION(BlueprintPure, Category = "RS|Interaction")
	float GetHoldProgress() const { return HoldProgress; }

	/** Written by the interaction ability while a hold runs. */
	void SetHoldProgress(float NewProgress);

	/**
	 * One trace, now, instead of waiting for the next timer fire.
	 *
	 * The server calls this when an activation arrives and its own cache is empty: at ten a
	 * second the cache can be up to 100 ms stale, and a player who pressed the key the frame
	 * after walking up to a locker would otherwise have a perfectly legitimate interaction
	 * refused. It is called once per activation, never per frame.
	 */
	void RefreshFocus();

	/**
	 * Switches the focus search off or back on. Off, the trace stops and the focus is cleared, which
	 * takes the prompt off the HUD through the usual delegates; `RefreshFocus` finds nothing.
	 *
	 * The pawn turns it off while its player is down. This component never learns what being down
	 * is: it only knows whether anyone wants it looking.
	 */
	void SetFocusEnabled(bool bEnabled);

	/** Fired when the focus changes, so the HUD never polls. */
	UPROPERTY(BlueprintAssignable, Category = "RS|Interaction")
	FRSInteractionFocusChanged OnFocusChanged;

	/** Fired while a hold advances. */
	UPROPERTY(BlueprintAssignable, Category = "RS|Interaction")
	FRSInteractionHoldProgress OnHoldProgressChanged;

	/**
	 * The text the HUD shows for the focus, as of the last trace; empty when nothing is focused.
	 * Kept on the locally controlled pawn only.
	 */
	const FText& GetCurrentPrompt() const { return FocusPrompt; }

	/**
	 * The prompt's text changed, on the locally controlled pawn: the focus moved, or what the
	 * focused interactable says about itself did. Checked on every focus trace, so at most one
	 * trace interval late, and never fired for the same text twice.
	 */
	FRSInteractionPromptChanged OnFocusPromptChanged;

protected:
	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	/** Traces from the pawn's view point and returns the interactable in front of it, if any. */
	AActor* TraceForInteractable() const;

	/** Whether this instance keeps a focus at all: the owning client draws it, the server rules on it. */
	bool ShouldTrack() const;

	/** How far in front of the view point an interactable can be used. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Interaction", meta = (ForceUnits = cm, ClampMin = 0.0))
	float MaxInteractionDistance = 250.0f;

	/**
	 * Radius of the swept sphere.
	 *
	 * A line through the exact centre of the reticle makes a small target feel unresponsive; the
	 * sphere is what lets a doorway-sized thing be used without pixel-hunting it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Interaction", meta = (ForceUnits = cm, ClampMin = 0.0))
	float TraceRadius = 12.0f;

	/** Seconds between focus traces. Ten a second is well under what a player can notice. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Interaction", meta = (ForceUnits = s, ClampMin = 0.01))
	float FocusRefreshInterval = 0.1f;

private:
	/** Walks up from a hit actor to the first actor implementing the interface, if any. */
	static AActor* ResolveInteractable(AActor* HitActor);

	/** Re-asks the focus for its prompt and broadcasts if the text differs. Locally controlled pawn only. */
	void RefreshPrompt();

	/** Starts the looping trace, with the first fire somewhere inside the interval. */
	void StartFocusTimer();

	/** Sets the focus and broadcasts if it changed. */
	void SetFocus(AActor* NewFocus);

	bool bFocusEnabled = true;

	TWeakObjectPtr<AActor> FocusedActor;

	FText FocusPrompt;

	float HoldProgress = 0.0f;

	FTimerHandle FocusTimerHandle;
};
