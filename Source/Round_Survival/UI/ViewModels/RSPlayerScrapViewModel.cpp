// Round Survival

#include "UI/ViewModels/RSPlayerScrapViewModel.h"

void URSPlayerScrapViewModel::SetPlayerName(const FString& InPlayerName)
{
	if (InPlayerName != CachedPlayerName)
	{
		CachedPlayerName = InPlayerName;
		UE_MVVM_SET_PROPERTY_VALUE(PlayerName, FText::FromString(InPlayerName));
	}
}

void URSPlayerScrapViewModel::SetScrap(float InScrap)
{
	// Scrap is an attribute, so a float; it only ever holds whole amounts.
	const int32 Scrap = FMath::RoundToInt(InScrap);
	if (Scrap != CachedScrap)
	{
		CachedScrap = Scrap;
		UE_MVVM_SET_PROPERTY_VALUE(ScrapText, FText::AsNumber(Scrap));
	}
}

void URSPlayerScrapViewModel::SetIsLocalPlayer(bool bInIsLocalPlayer)
{
	bIsLocalPlayer = bInIsLocalPlayer;
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerMarkerVisibility,
		bInIsLocalPlayer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
