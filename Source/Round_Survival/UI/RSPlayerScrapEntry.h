// Round Survival

#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonUserWidget.h"

#include "RSPlayerScrapEntry.generated.h"

class UPanelWidget;
class URSPlayerScrapViewModel;
class URSScrapPopup;
struct FGameplayTag;

/**
 * One row of the scrap list.
 *
 * The list view hands each row a URSPlayerScrapViewModel as its list item, and this sets it on the
 * row's MVVM view — the row's Blueprint declares that viewmodel as Manual and binds it like any
 * other. A row reused for another player simply gets the other player's viewmodel.
 *
 * The local player's row also floats each change to its balance off the number: every row hears
 * URSHUDSubsystem::OnLocalScrapDelta, and the one whose viewmodel is the local player's shows it,
 * asking at that moment rather than remembering, so a row reassigned in between cannot be wrong.
 * Popups go in PopupLayer and are reused once idle.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSPlayerScrapEntry : public UCommonUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End of UUserWidget interface

	//~IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End of IUserObjectListEntry interface

	/** Where the popups go: a zero-width panel beside the balance, so they never push the row around. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Scrap", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> PopupLayer;

	UPROPERTY(EditAnywhere, Category = "RS|Scrap")
	TSubclassOf<URSScrapPopup> PopupClass;

	/**
	 * Popups closer together than this are one burst — a hit and the kill it caused, in one frame —
	 * and stack upwards instead of rising on top of each other.
	 */
	UPROPERTY(EditAnywhere, Category = "RS|Scrap", meta = (ClampMin = 0.0, Units = "s"))
	float BurstWindow = 0.1f;

	/** How far above the previous one each popup of a burst starts. */
	UPROPERTY(EditAnywhere, Category = "RS|Scrap", meta = (ClampMin = 0.0))
	float BurstSpacing = 22.0f;

private:
	void HandleLocalScrapDelta(int32 Delta, const FGameplayTag& Reason);

	double LastPopupTime = -1.0;
	int32 BurstIndex = 0;

	TWeakObjectPtr<URSPlayerScrapViewModel> ViewModel;
	FDelegateHandle ScrapDeltaHandle;
};
