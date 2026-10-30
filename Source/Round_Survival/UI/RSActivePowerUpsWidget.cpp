// Round Survival

#include "UI/RSActivePowerUpsWidget.h"

#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "UI/RSActivePowerUpEntry.h"

void URSActivePowerUpsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The designer keeps it visible to lay it out.
	if (!IsDesignTime())
	{
		Redraw();
	}
}

void URSActivePowerUpsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	Redraw();
}

void URSActivePowerUpsWidget::SetActivePowerUps(const TArray<FRSActivePowerUp>& InActivePowerUps)
{
	ActivePowerUps = InActivePowerUps;

	if (!EntryPanel || !EntryClass || IsDesignTime())
	{
		return;
	}

	// One entry per power-up, kept in order. Entries are reused from the front, so a power-up that
	// runs out shifts the ones after it along rather than leaving a gap.
	while (Entries.Num() > ActivePowerUps.Num())
	{
		Entries.Pop()->RemoveFromParent();
	}

	while (Entries.Num() < ActivePowerUps.Num())
	{
		URSActivePowerUpEntry* Entry = CreateWidget<URSActivePowerUpEntry>(this, EntryClass);
		EntryPanel->AddChild(Entry);
		Entries.Add(Entry);
	}

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index]->SetPowerUp(ActivePowerUps[Index]);
	}

	// Drawn now rather than on the next tick, so a new entry never shows a frame without its number.
	Redraw();
}

void URSActivePowerUpsWidget::Redraw()
{
	if (IsDesignTime())
	{
		return;
	}

	const ESlateVisibility WantedVisibility = Entries.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	if (GetVisibility() != WantedVisibility)
	{
		SetVisibility(WantedVisibility);
	}

	// The clock the ability system measures an active effect's duration on.
	const UWorld* World = GetWorld();
	const double WorldTime = World ? World->GetTimeSeconds() : 0.0;

	for (URSActivePowerUpEntry* Entry : Entries)
	{
		Entry->Redraw(WorldTime);
	}
}
