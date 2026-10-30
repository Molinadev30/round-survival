// Round Survival

#pragma once

#include "CommonUserWidget.h"
#include "UI/ViewModels/RSPowerUpsViewModel.h"

#include "RSActivePowerUpEntry.generated.h"

class UImage;
class UTextBlock;

/**
 * One timed power-up on the HUD: a swatch of its colour and the whole seconds it has left.
 *
 * Made and placed by URSActivePowerUpsWidget, which hands it its power-up and redraws it each frame.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSActivePowerUpEntry : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetPowerUp(const FRSActivePowerUp& InPowerUp);

	/** Draws the seconds left at WorldTime. */
	void Redraw(double WorldTime);

protected:
	/** Tinted with the power-up's colour. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Power-Up", meta = (BindWidget))
	TObjectPtr<UImage> Swatch;

	/** Whole seconds left, rounded up: it reads 1 until the moment it runs out. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Power-Up", meta = (BindWidget))
	TObjectPtr<UTextBlock> SecondsText;

private:
	FRSActivePowerUp PowerUp;

	/** What SecondsText holds, so the text is only rebuilt when the number changes. */
	int32 ShownSeconds = INDEX_NONE;
};
