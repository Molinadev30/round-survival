// Round Survival

#include "UI/ViewModels/RSDownedViewModel.h"

#define LOCTEXT_NAMESPACE "RSDownedViewModel"

void URSDownedViewModel::SetDowned(const FRSBleedOutClock& InClock, bool bBeingRevived)
{
	// Handed on every time rather than compared: it changes a handful of times per down, and a
	// redundant one costs the widget one redraw.
	BleedOutClock = InClock;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BleedOutClock);

	const EStatus Status = InClock.bSelfRevive ? EStatus::GettingUp : (bBeingRevived ? EStatus::BeingRevived : EStatus::BleedingOut);

	if (CachedStatus != Status)
	{
		CachedStatus = Status;

		switch (Status)
		{
		case EStatus::GettingUp:
			StatusLabel = LOCTEXT("GettingUp", "Getting back up");
			break;
		case EStatus::BeingRevived:
			StatusLabel = LOCTEXT("BeingRevived", "Being revived");
			break;
		default:
			StatusLabel = LOCTEXT("BleedingOut", "Bleeding out");
			break;
		}

		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StatusLabel);
	}
}

void URSDownedViewModel::ClearDowned()
{
	SetDowned(FRSBleedOutClock(), false);
}

#undef LOCTEXT_NAMESPACE
