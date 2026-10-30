// Round Survival

#include "UI/RSActivePowerUpEntry.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void URSActivePowerUpEntry::SetPowerUp(const FRSActivePowerUp& InPowerUp)
{
	PowerUp = InPowerUp;

	if (Swatch)
	{
		Swatch->SetColorAndOpacity(PowerUp.Color);
	}
}

void URSActivePowerUpEntry::Redraw(double WorldTime)
{
	const int32 Seconds = FMath::CeilToInt(FMath::Max(0.0, PowerUp.EndWorldTime - WorldTime));
	if (SecondsText && Seconds != ShownSeconds)
	{
		ShownSeconds = Seconds;
		SecondsText->SetText(FText::AsNumber(Seconds));
	}
}
