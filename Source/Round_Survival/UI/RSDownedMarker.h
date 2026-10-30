// Round Survival

#pragma once

#include "CommonUserWidget.h"
#include "Core/RSPlayerState.h"
#include "Curves/CurveLinearColor.h"

#include "RSDownedMarker.generated.h"

class UImage;
class URSDownedTeammateViewModel;

/**
 * The marker over one downed teammate: an icon whose colour runs from white to red as their
 * bleed-out does, their name, and "Reviving" in its own colour while someone is.
 *
 * Placed by URSDownedMarkerLayer, which hands it its URSDownedTeammateViewModel; the Blueprint
 * declares that viewmodel as Manual and binds the name, the clock and the revive to it. The colour
 * moves with the clock, so the layer redraws it each frame along with the position, against the
 * server's time.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSDownedMarker : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	URSDownedMarker(const FObjectInitializer& ObjectInitializer);

	/** Sets the teammate on the marker's MVVM view. */
	void SetTeammate(URSDownedTeammateViewModel* InTeammate);

	URSDownedTeammateViewModel* GetTeammate() const { return Teammate.Get(); }

	/** Bound from the view model. */
	UFUNCTION(BlueprintCallable, Category = "RS|Downed Marker")
	void SetBleedOutClock(const FRSBleedOutClock& InClock);

	/** Bound from the view model. */
	UFUNCTION(BlueprintCallable, Category = "RS|Downed Marker")
	void SetBeingRevived(bool bInBeingRevived);

	/** Colours the icon for the clock as it stands at ServerTime. Called by the layer every frame. */
	void Redraw(double ServerTime);

protected:
	UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetBleedOutClock, Category = "RS|Downed Marker")
	FRSBleedOutClock BleedOutClock;

	UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetBeingRevived, Category = "RS|Downed Marker")
	bool bBeingRevived = false;

	/** The icon's colour by the share of the bleed-out left, 1 just down and 0 run out. */
	UPROPERTY(EditAnywhere, Category = "RS|Downed Marker")
	FRuntimeCurveLinearColor ColorByTimeLeft;

	/** The icon's colour while someone is reviving, whatever the clock. */
	UPROPERTY(EditAnywhere, Category = "RS|Downed Marker")
	FLinearColor RevivingColor = FLinearColor(0.35f, 0.8f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "RS|Downed Marker", meta = (BindWidget))
	TObjectPtr<UImage> Icon;

	/** Shown while someone is reviving. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Downed Marker", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RevivingLabel;

private:
	TWeakObjectPtr<URSDownedTeammateViewModel> Teammate;
};
