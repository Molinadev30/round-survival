// Round Survival

#pragma once

#include "CommonUserWidget.h"
#include "Curves/CurveFloat.h"

#include "RSDamageVignetteWidget.generated.h"

class URSDamageVignetteImage;
class UWidgetAnimation;

/**
 * The red that creeps in from the screen's edges as the player on screen loses health, and flashes
 * on every hit: the local player, or a teammate a dead player watches through their eyes.
 *
 * Two inputs, two routes. How hurt the player is is state: it arrives by binding from
 * URSHealthViewModel and is turned into the vignette's depth through a curve of health fraction,
 * tuned so that each of an enemy's hits reads as a step deeper than the last. Being hit is a moment: it
 * arrives as URSHUDSubsystem::OnViewedPlayerDamaged and plays the HitPulse animation from the
 * start, so a second hit mid-flash restarts it rather than stacking. Nothing here ticks;
 * regeneration moves the vignette back out because it moves health, ten times a second.
 *
 * Meant to cover the whole screen, under everything else on the HUD.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSDamageVignetteWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	URSDamageVignetteWidget(const FObjectInitializer& ObjectInitializer);

	/** 0 at full health, 1 at none. Bound from the view model. */
	UFUNCTION(BlueprintCallable, Category = "RS|Damage Vignette")
	void SetMissingHealth(float InMissingHealth);

protected:
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End of UUserWidget interface

	UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetMissingHealth, Category = "RS|Damage Vignette")
	float MissingHealth = 0.0f;

	/**
	 * The vignette's depth, 0 to 1, by health fraction, 0 to 1. A curve rather than two thresholds
	 * because the steps matter more than the line: with a 40-point hit on 100 health, the player
	 * stands at 60 %, 20 % and 0 % after each hit, and each has to look worse than the last. The
	 * defaults put those at 0.15, 0.33 and 1, and nothing from 80 % up.
	 */
	UPROPERTY(EditAnywhere, Category = "RS|Damage Vignette")
	FRuntimeFloatCurve IntensityByHealth;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Damage Vignette", meta = (BindWidget))
	TObjectPtr<URSDamageVignetteImage> Vignette;

	/** Keys the vignette's Pulse from 1 back to 0. Without it a hit shows only as lost health. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Damage Vignette", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HitPulse;

private:
	void HandleViewedPlayerDamaged();

	FDelegateHandle DamagedHandle;
};
