// Round Survival

#pragma once

#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "RSPlayerScrapViewModel.generated.h"

/**
 * One row of the scrap list: a player's name and balance.
 *
 * One per player in the match, owned by URSScrapBoardViewModel and handed to each list entry
 * widget as its list item — so this one is set on the widget manually, never resolved.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Manual"))
class ROUND_SURVIVAL_API URSPlayerScrapViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetPlayerName(const FString& InPlayerName);
	void SetScrap(float InScrap);

	/** The row of the player looking at the HUD shows a marker, so they find their own balance at a glance. */
	void SetIsLocalPlayer(bool bInIsLocalPlayer);

	/** Asked by the row when a scrap popup arrives: only the local player's row shows them. */
	bool IsLocalPlayer() const { return bIsLocalPlayer; }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Scrap", meta = (AllowPrivateAccess = true))
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Scrap", meta = (AllowPrivateAccess = true))
	FText ScrapText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Scrap", meta = (AllowPrivateAccess = true))
	ESlateVisibility LocalPlayerMarkerVisibility = ESlateVisibility::Hidden;

	/** What the texts were last built from. FText compares by identity, so rebuilding unchanged text would notify every binding. */
	FString CachedPlayerName;
	int32 CachedScrap = INDEX_NONE;
	bool bIsLocalPlayer = false;
};
