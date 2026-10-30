// Round Survival

#include "UI/ViewModels/RSRoundViewModel.h"

#define LOCTEXT_NAMESPACE "RSRoundViewModel"

void URSRoundViewModel::SetRound(int32 RoundNumber, ERSRoundPhase Phase)
{
	// Texts are rebuilt only when what they show changes: FText compares by identity, so a freshly
	// formatted copy of the same number would notify every binding for nothing.
	if (RoundNumber != CachedRoundNumber)
	{
		CachedRoundNumber = RoundNumber;
		UE_MVVM_SET_PROPERTY_VALUE(RoundText, FText::AsNumber(RoundNumber));
		UE_MVVM_SET_PROPERTY_VALUE(RoundVisibility,
			RoundNumber > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Phase != CachedPhase)
	{
		CachedPhase = Phase;

		switch (Phase)
		{
		case ERSRoundPhase::Intermission:
			UE_MVVM_SET_PROPERTY_VALUE(PhaseText, LOCTEXT("Intermission", "Intermission"));
			break;
		default:
			// Game over included: the game over screen says it, in the middle of the screen.
			UE_MVVM_SET_PROPERTY_VALUE(PhaseText, FText::GetEmpty());
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE
