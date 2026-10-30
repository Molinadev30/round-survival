// Round Survival

#include "UI/ViewModels/RSSpectatorViewModel.h"

#define LOCTEXT_NAMESPACE "RSSpectatorViewModel"

void URSSpectatorViewModel::SetSpectating(const FText& InViewedPlayerName, ERSSpectatorView View, const FText& InNextKeyText, const FText& InToggleViewKeyText)
{
	// Compared by content: FText's own comparison is by identity, so the same name handed over
	// again would notify every binding for nothing.
	if (!ViewedPlayerName.EqualTo(InViewedPlayerName))
	{
		ViewedPlayerName = InViewedPlayerName;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ViewedPlayerName);
	}

	if (!NextKeyText.EqualTo(InNextKeyText))
	{
		NextKeyText = InNextKeyText;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(NextKeyText);
	}

	if (!ToggleViewKeyText.EqualTo(InToggleViewKeyText))
	{
		ToggleViewKeyText = InToggleViewKeyText;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ToggleViewKeyText);
	}

	if (View != CachedView)
	{
		CachedView = View;
		ToggleViewLabel = View == ERSSpectatorView::FirstPerson
			? LOCTEXT("ToThirdPerson", "Third person")
			: LOCTEXT("ToFirstPerson", "First person");
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ToggleViewLabel);
	}

	UE_MVVM_SET_PROPERTY_VALUE(BannerVisibility, ESlateVisibility::HitTestInvisible);
}

void URSSpectatorViewModel::ClearSpectating()
{
	UE_MVVM_SET_PROPERTY_VALUE(BannerVisibility, ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
