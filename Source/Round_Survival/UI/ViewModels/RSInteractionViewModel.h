// Round Survival

#pragma once

#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "RSInteractionViewModel.generated.h"

/**
 * The interaction prompt — "Press E for Carbine [Cost: 1,000]" — and the hold bar under it.
 *
 * The sentence comes from the focused interactable with IRSInteractable::PromptKeyToken where the
 * key goes, and is held here in three pieces around that key, so the widget can style the key on
 * its own without the sentence knowing how. The prompt shows whether or not the interaction is
 * possible right now — a locker you cannot afford still names its price — and it looks the same
 * either way. Fed by URSHUDSubsystem from the pawn's URSInteractorComponent.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSInteractionViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/**
	 * The interactable's sentence and the key to put in it. Empty text hides the prompt; a sentence
	 * without the key token shows whole, with no key.
	 */
	void SetPrompt(const FText& InPrompt, const FText& InKeyText);

	/** 0 to 1 while a hold runs, 0 otherwise; anything above zero shows the bar. */
	void SetHoldProgress(float InHoldProgress);

private:
	/** The sentence up to the key, or all of it when it names no key. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Interaction", meta = (AllowPrivateAccess = true))
	FText PromptBeforeKey;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Interaction", meta = (AllowPrivateAccess = true))
	FText KeyText;

	/** The sentence after the key. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Interaction", meta = (AllowPrivateAccess = true))
	FText PromptAfterKey;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Interaction", meta = (AllowPrivateAccess = true))
	ESlateVisibility KeyVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Interaction", meta = (AllowPrivateAccess = true))
	ESlateVisibility PromptVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Interaction", meta = (AllowPrivateAccess = true))
	float HoldProgress = 0.0f;

	/** Hidden rather than collapsed, so the prompt does not shift when the bar comes and goes. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Interaction", meta = (AllowPrivateAccess = true))
	ESlateVisibility HoldVisibility = ESlateVisibility::Hidden;
};
