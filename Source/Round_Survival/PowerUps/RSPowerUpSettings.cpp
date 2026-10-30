// Round Survival

#include "PowerUps/RSPowerUpSettings.h"

#include "PowerUps/RSPowerUpDefinition.h"

const URSPowerUpDefinition* URSPowerUpSettings::PickPowerUp() const
{
	float TotalWeight = 0.0f;
	for (const FRSPowerUpDrop& Drop : PowerUps)
	{
		if (Drop.PowerUp && Drop.Weight > 0.0f)
		{
			TotalWeight += Drop.Weight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Pick = FMath::FRandRange(0.0f, TotalWeight);
	const URSPowerUpDefinition* Last = nullptr;

	for (const FRSPowerUpDrop& Drop : PowerUps)
	{
		if (!Drop.PowerUp || Drop.Weight <= 0.0f)
		{
			continue;
		}

		Last = Drop.PowerUp;
		Pick -= Drop.Weight;

		if (Pick <= 0.0f)
		{
			return Drop.PowerUp;
		}
	}

	// Float rounding at the very top of the range lands here: the last entry owns it.
	return Last;
}

const URSPowerUpDefinition* URSPowerUpSettings::FindPowerUp(const FString& Name) const
{
	for (const FRSPowerUpDrop& Drop : PowerUps)
	{
		if (Drop.PowerUp && Drop.PowerUp->GetName().Contains(Name))
		{
			return Drop.PowerUp;
		}
	}

	return nullptr;
}
