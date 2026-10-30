// Round Survival

#include "UI/ViewModels/RSPlayerMatchStatsViewModel.h"

#include "Core/RSPlayerState.h"

void URSPlayerMatchStatsViewModel::SetPlayerName(const FString& InPlayerName)
{
	if (InPlayerName != CachedPlayerName)
	{
		CachedPlayerName = InPlayerName;
		UE_MVVM_SET_PROPERTY_VALUE(PlayerName, FText::FromString(InPlayerName));
	}
}

void URSPlayerMatchStatsViewModel::SetStats(int32 InScore, const FRSPlayerMatchStats& InStats)
{
	using FFields = ThisClass::FFieldNotificationClassDescriptor;

	SetNumber(CachedScore, InScore, ScoreText, FFields::ScoreText);
	SetNumber(CachedKills, InStats.Kills, KillsText, FFields::KillsText);
	SetNumber(CachedHeadshots, InStats.Headshots, HeadshotsText, FFields::HeadshotsText);
	SetNumber(CachedDowns, InStats.Downs, DownsText, FFields::DownsText);
	SetNumber(CachedRevives, InStats.Revives, RevivesText, FFields::RevivesText);
}

void URSPlayerMatchStatsViewModel::SetIsLocalPlayer(bool bInIsLocalPlayer)
{
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerMarkerVisibility,
		bInIsLocalPlayer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void URSPlayerMatchStatsViewModel::SetNumber(int32& Cached, int32 NewValue, FText& Text, const UE::FieldNotification::FFieldId& FieldId)
{
	if (NewValue != Cached)
	{
		Cached = NewValue;

		// Grouped like the scrap list's balance: a score runs into the thousands.
		Text = FText::AsNumber(NewValue);
		BroadcastFieldValueChanged(FieldId);
	}
}
