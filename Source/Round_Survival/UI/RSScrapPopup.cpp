// Round Survival

#include "UI/RSScrapPopup.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"

void URSScrapPopup::NativeConstruct()
{
	Super::NativeConstruct();

	if (!bShowing)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URSScrapPopup::Show(int32 Delta, const FGameplayTag& InReason)
{
	Reason = InReason;
	bShowing = true;

	// AsNumber groups the thousands and brings the minus sign; the plus is ours.
	const FText Number = FText::AsNumber(Delta);
	AmountText->SetText(Delta > 0 ? FText::Format(NSLOCTEXT("RSScrapPopup", "Award", "+{0}"), Number) : Number);
	AmountText->SetColorAndOpacity(Delta > 0 ? AwardColor : SpendColor);

	SetVisibility(ESlateVisibility::HitTestInvisible);
	PlayAnimation(Rise);
}

void URSScrapPopup::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation == Rise)
	{
		bShowing = false;
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
