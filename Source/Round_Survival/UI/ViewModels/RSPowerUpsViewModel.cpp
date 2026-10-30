// Round Survival

#include "UI/ViewModels/RSPowerUpsViewModel.h"

void URSPowerUpsViewModel::SetActivePowerUps(const TArray<FRSActivePowerUp>& InActivePowerUps)
{
	if (ActivePowerUps == InActivePowerUps)
	{
		return;
	}

	ActivePowerUps = InActivePowerUps;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ActivePowerUps);
}
