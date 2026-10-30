// Round Survival

#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"

#include "RSScrapPopup.generated.h"

class UTextBlock;
class UWidgetAnimation;

/**
 * One "+60" or "-1,000" floating off the local player's scrap: plays its Rise animation once and
 * collapses itself at the end, ready to be shown again. The scrap row keeps a few and reuses
 * whichever is idle, so a burst of awards costs no new widgets after the first.
 *
 * Awards and spending differ only in colour. What the change was for arrives as Reason — an
 * RS.Economy tag — and is kept for the Blueprint to style by.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSScrapPopup : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Shows Delta from the start of the animation, over whatever this popup was showing. */
	void Show(int32 Delta, const FGameplayTag& InReason);

	/** True from Show until the animation ends. */
	bool IsShowing() const { return bShowing; }

protected:
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
	//~End of UUserWidget interface

	UPROPERTY(EditAnywhere, Category = "RS|Scrap Popup")
	FLinearColor AwardColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "RS|Scrap Popup")
	FLinearColor SpendColor = FLinearColor(0.9f, 0.12f, 0.1f, 1.0f);

	/** What the change shown was for: an RS.Economy tag, or none. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Scrap Popup")
	FGameplayTag Reason;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Scrap Popup", meta = (BindWidget))
	TObjectPtr<UTextBlock> AmountText;

	/** Rises and fades out. Its end is the popup's end. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RS|Scrap Popup", meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Rise;

private:
	bool bShowing = false;
};
