// Round Survival

#include "UI/ViewModels/RSDownedTeammateViewModel.h"

void URSDownedTeammateViewModel::SetPlayerName(const FString& InPlayerName)
{
	if (CachedPlayerName == InPlayerName && !PlayerName.IsEmpty())
	{
		return;
	}

	CachedPlayerName = InPlayerName;
	PlayerName = FText::FromString(InPlayerName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PlayerName);
}

void URSDownedTeammateViewModel::SetDowned(const FRSBleedOutClock& InClock, bool bInBeingRevived)
{
	// Handed on every time, like URSDownedViewModel's: a handful of changes per down.
	BleedOutClock = InClock;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BleedOutClock);

	UE_MVVM_SET_PROPERTY_VALUE(bBeingRevived, bInBeingRevived);
}
