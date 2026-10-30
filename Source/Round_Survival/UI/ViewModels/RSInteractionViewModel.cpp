// Round Survival

#include "UI/ViewModels/RSInteractionViewModel.h"

#include "Interaction/RSInteractable.h"

void URSInteractionViewModel::SetPrompt(const FText& InPrompt, const FText& InKeyText)
{
	// The interactor only reports a prompt whose content changed, so every piece is set as given.
	// The pieces are trimmed; the widget spaces the key with padding, since a text block measures
	// trailing spaces inconsistently.
	FString Before = InPrompt.ToString();
	FString After;
	const bool bHasKey = Before.Split(IRSInteractable::PromptKeyToken, &Before, &After);

	PromptBeforeKey = FText::FromString(Before.TrimEnd());
	PromptAfterKey = FText::FromString(After.TrimStart());
	KeyText = bHasKey ? InKeyText : FText::GetEmpty();

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PromptBeforeKey);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PromptAfterKey);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KeyText);

	UE_MVVM_SET_PROPERTY_VALUE(KeyVisibility, bHasKey ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(PromptVisibility,
		InPrompt.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void URSInteractionViewModel::SetHoldProgress(float InHoldProgress)
{
	UE_MVVM_SET_PROPERTY_VALUE(HoldProgress, InHoldProgress);
	UE_MVVM_SET_PROPERTY_VALUE(HoldVisibility,
		InHoldProgress > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
