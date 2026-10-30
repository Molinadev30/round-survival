// Round Survival

#include "UI/ViewModels/RSGameOverViewModel.h"

#include "Algo/Compare.h"
#include "UI/ViewModels/RSPlayerMatchStatsViewModel.h"

#define LOCTEXT_NAMESPACE "RSGameOverViewModel"

void URSGameOverViewModel::SetGameOver(int32 RoundReached, bool bCanRestart, const FText& InRestartKeyText)
{
	// Rebuilt only when the number changes: FText compares by identity, so a freshly formatted copy
	// of the same sentence would notify every binding for nothing.
	if (RoundReached != CachedRoundReached)
	{
		CachedRoundReached = RoundReached;
		UE_MVVM_SET_PROPERTY_VALUE(SurvivedText, FText::Format(
			LOCTEXT("Survived", "You survived {0} {0}|plural(one=round,other=rounds)"), RoundReached));
	}

	// Without a key bound there is nothing to press, but the host can still restart from the console.
	const FText NewRestartText = !bCanRestart
		? LOCTEXT("WaitingForHost", "Waiting for the host to restart")
		: InRestartKeyText.IsEmpty()
			? LOCTEXT("RestartNoKey", "Restart with RS.RestartMatch")
			: FText::Format(LOCTEXT("PressToRestart", "Press {0} to restart"), InRestartKeyText);

	// Compared by content, for the same reason as above.
	if (!RestartText.EqualTo(NewRestartText))
	{
		RestartText = NewRestartText;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RestartText);
	}

	UE_MVVM_SET_PROPERTY_VALUE(ScreenVisibility, ESlateVisibility::HitTestInvisible);
}

void URSGameOverViewModel::ClearGameOver()
{
	UE_MVVM_SET_PROPERTY_VALUE(ScreenVisibility, ESlateVisibility::Collapsed);
}

void URSGameOverViewModel::SetRows(const TArray<URSPlayerMatchStatsViewModel*>& InRows)
{
	// A list view rebuilds its rows on every notification, so an unchanged order sends nothing.
	const bool bSame = Rows.Num() == InRows.Num()
		&& Algo::Compare(Rows, InRows, [](const UObject* Row, const UObject* InRow) { return Row == InRow; });

	if (!bSame)
	{
		Rows = TArray<TObjectPtr<UObject>>(InRows);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Rows);
	}
}

#undef LOCTEXT_NAMESPACE
