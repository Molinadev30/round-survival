// Round Survival

#pragma once

#include "CommonUserWidget.h"

#include "RSDownedMarkerLayer.generated.h"

class UCanvasPanel;
class URSDownedMarker;
class URSDownedTeammateViewModel;

/**
 * Puts a URSDownedMarker over every downed teammate, seen through walls, and pins it to the edge of
 * the screen when the teammate is off it or behind the camera, on the side they are on.
 *
 * Who is down arrives by binding from URSDownedTeammatesViewModel, and only when it changes: a marker
 * is made for each teammate added and removed with them. Where each one goes changes with every
 * frame the camera moves, so placing them is done on tick: the teammate's pawn is projected through
 * the local player's own view, whatever it is looking through — their pawn, or a teammate they
 * spectate — and a point off the screen is pushed back along its direction from the centre to an
 * inset rectangle. The layer collapses itself while nobody is down, and a collapsed widget does not
 * tick.
 *
 * Meant to cover the whole screen, under the rest of the HUD.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSDownedMarkerLayer : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Bound from the view model. */
	UFUNCTION(BlueprintCallable, Category = "RS|Downed Markers")
	void SetTeammates(const TArray<URSDownedTeammateViewModel*>& InTeammates);

protected:
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~End of UUserWidget interface

	UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetTeammates, Category = "RS|Downed Markers")
	TArray<TObjectPtr<URSDownedTeammateViewModel>> Teammates;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Downed Markers", meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MarkerCanvas;

	UPROPERTY(EditAnywhere, Category = "RS|Downed Markers")
	TSubclassOf<URSDownedMarker> MarkerClass;

	/** How far above the downed pawn's origin the marker points. A downed player lies low. */
	UPROPERTY(EditAnywhere, Category = "RS|Downed Markers", meta = (ForceUnits = cm))
	float HeightAboveTeammate = 40.0f;

	/** How far in from the screen's edges a marker is pinned when its teammate is off screen. */
	UPROPERTY(EditAnywhere, Category = "RS|Downed Markers", meta = (ClampMin = 0.0))
	float EdgeMargin = 48.0f;

private:
	/** Positions and colours every marker for the current view. */
	void PlaceMarkers();

	/** Pinned to the inset rectangle along Direction from the centre. */
	static FVector2D PinToEdge(const FVector2D& Center, const FVector2D& HalfExtent, FVector2D Direction);

	UPROPERTY(Transient)
	TArray<TObjectPtr<URSDownedMarker>> Markers;
};
