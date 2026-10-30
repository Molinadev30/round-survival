// Round Survival

#include "UI/RSDownedStatusWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void URSDownedStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The designer keeps it visible to lay it out.
	if (!IsDesignTime())
	{
		Redraw();
	}
}

void URSDownedStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	Redraw();
}

void URSDownedStatusWidget::SetBleedOutClock(const FRSBleedOutClock& InClock)
{
	BleedOutClock = InClock;
	Redraw();
}

void URSDownedStatusWidget::SetStatusLabel(const FText& InStatusLabel)
{
	StatusLabel = InStatusLabel;

	if (StatusText)
	{
		StatusText->SetText(StatusLabel);
	}
}

void URSDownedStatusWidget::Redraw()
{
	if (IsDesignTime())
	{
		return;
	}

	const ESlateVisibility WantedVisibility = BleedOutClock.IsActive() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (GetVisibility() != WantedVisibility)
	{
		SetVisibility(WantedVisibility);
	}

	if (!BleedOutClock.IsActive())
	{
		ShownSeconds = INDEX_NONE;
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const double ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);

	const float SecondsLeft = BleedOutClock.GetSecondsLeft(ServerTime);

	if (TimeBar)
	{
		TimeBar->SetPercent(SecondsLeft / BleedOutClock.Duration);
	}

	const int32 Seconds = FMath::CeilToInt(SecondsLeft);
	if (SecondsText && Seconds != ShownSeconds)
	{
		ShownSeconds = Seconds;
		SecondsText->SetText(FText::AsNumber(Seconds));
	}
}
