// Round Survival

#pragma once

#include "CommonUserWidget.h"
#include "Core/RSPlayerState.h"

#include "RSDownedStatusWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * What a downed player sees of their bleed-out: the seconds left and a bar that empties, under a label
 * the view model words — "Bleeding out", or "Being revived" while a teammate holds the clock, which
 * then stands still here as it does on the server.
 *
 * The clock arrives by binding from URSDownedViewModel, and only when it changes: when the player goes
 * down, a revive starts or stops, or the bleed-out ends. Between those it is a fixed moment on the
 * server's clock, and turning it into seconds left is drawing, not state, so it is done on tick
 * against AGameStateBase::GetServerWorldTimeSeconds. The widget shows itself only while the clock is
 * active, and a collapsed widget does not tick, so standing players pay nothing for it.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSDownedStatusWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Bound from the view model. */
	UFUNCTION(BlueprintCallable, Category = "RS|Downed Status")
	void SetBleedOutClock(const FRSBleedOutClock& InClock);

	/** Bound from the view model. */
	UFUNCTION(BlueprintCallable, Category = "RS|Downed Status")
	void SetStatusLabel(const FText& InStatusLabel);

protected:
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~End of UUserWidget interface

	UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetBleedOutClock, Category = "RS|Downed Status")
	FRSBleedOutClock BleedOutClock;

	UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetStatusLabel, Category = "RS|Downed Status")
	FText StatusLabel;

	/** Shows StatusLabel. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Downed Status", meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** Whole seconds left, rounded up: it reads 1 until the moment it runs out. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Downed Status", meta = (BindWidget))
	TObjectPtr<UTextBlock> SecondsText;

	/** Full when the player goes down, empty when they bleed out. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Downed Status", meta = (BindWidget))
	TObjectPtr<UProgressBar> TimeBar;

private:
	/** Shows or hides the widget for the clock, and draws it as it stands now. */
	void Redraw();

	/** What SecondsText holds, so the text is only rebuilt when the number changes. */
	int32 ShownSeconds = INDEX_NONE;
};
