// Round Survival

#pragma once

#include "CommonUserWidget.h"
#include "UI/ViewModels/RSPowerUpsViewModel.h"

#include "RSActivePowerUpsWidget.generated.h"

class UPanelWidget;
class URSActivePowerUpEntry;

/**
 * The team's timed power-ups, one URSActivePowerUpEntry each, side by side.
 *
 * Which ones and when each runs out arrive by binding from URSPowerUpsViewModel, and only when that
 * changes: a pickup, a second pickup restarting one, one running out. Turning an end time into
 * seconds left is drawing, not state, so it is done on tick, like the bleed-out countdown. The widget
 * collapses itself while there are none, and a collapsed widget does not tick.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSActivePowerUpsWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Bound from the view model. */
	UFUNCTION(BlueprintCallable, Category = "RS|Power-Up")
	void SetActivePowerUps(const TArray<FRSActivePowerUp>& InActivePowerUps);

protected:
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~End of UUserWidget interface

	UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetActivePowerUps, Category = "RS|Power-Up")
	TArray<FRSActivePowerUp> ActivePowerUps;

	/** Where the entries go, in order. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Power-Up", meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryPanel;

	UPROPERTY(EditAnywhere, Category = "RS|Power-Up")
	TSubclassOf<URSActivePowerUpEntry> EntryClass;

private:
	/** Shows or hides the widget, and draws every entry as it stands now. */
	void Redraw();

	UPROPERTY(Transient)
	TArray<TObjectPtr<URSActivePowerUpEntry>> Entries;
};
