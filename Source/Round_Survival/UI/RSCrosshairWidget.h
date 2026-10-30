// Round Survival

#pragma once

#include "CommonUserWidget.h"

#include "RSCrosshairWidget.generated.h"

/**
 * Four ticks around the centre of the screen, spaced by the drawn weapon's real spread.
 *
 * The gap is the cone the next shot will land in, not an animation of one: firing opens it as the
 * weapon heats, it closes as it cools, and moving opens it through the weapon's own player-state
 * multipliers. The angle is what the fire ability draws its bullets from, projected through the
 * camera's field of view, so a target inside the ticks is a target the spread can reach.
 *
 * The spread changes every frame — it cools continuously — so this reads it every frame, which is
 * what Lyra's reticle does as well. It is one weapon lookup and four boxes; everything about the
 * look is a property tuned in the designer. Whether the centre is on an enemy is state rather than
 * a continuous value, and arrives by binding from URSCrosshairViewModel.
 *
 * Meant to cover the whole screen: it draws at its own centre and never clips the ticks.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSCrosshairWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RS|Crosshair")
	void SetAimingAtEnemy(bool bInAimingAtEnemy) { bAimingAtEnemy = bInAimingAtEnemy; }

protected:
	//~UUserWidget interface
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	//~End of UUserWidget interface

	/** True while the centre of the screen is on a living enemy. Bound from the view model. */
	UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetAimingAtEnemy, Category = "RS|Crosshair")
	bool bAimingAtEnemy = false;

	UPROPERTY(EditAnywhere, Category = "RS|Crosshair")
	FLinearColor Color = FLinearColor(0.95f, 0.95f, 0.95f, 0.9f);

	UPROPERTY(EditAnywhere, Category = "RS|Crosshair")
	FLinearColor EnemyColor = FLinearColor(0.9f, 0.1f, 0.08f, 1.0f);

	/** Drawn one unit around each tick, so the crosshair reads on a white wall as well as a dark one. */
	UPROPERTY(EditAnywhere, Category = "RS|Crosshair")
	FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	UPROPERTY(EditAnywhere, Category = "RS|Crosshair", meta = (ClampMin = 0.0))
	float TickLength = 10.0f;

	UPROPERTY(EditAnywhere, Category = "RS|Crosshair", meta = (ClampMin = 0.0))
	float TickThickness = 2.0f;

	/** Gap between the centre and each tick with no spread at all: first-shot accuracy, standing still. */
	UPROPERTY(EditAnywhere, Category = "RS|Crosshair", meta = (ClampMin = 0.0))
	float MinimumGap = 6.0f;

private:
	/** The spread cone's radius at the screen, in this widget's units, as of the last tick; negative with no ranged weapon drawn. */
	float SpreadRadius = -1.0f;

	/** Fades out as the player goes into the sights: aimed, the sights are the crosshair. */
	float Opacity = 1.0f;
};
