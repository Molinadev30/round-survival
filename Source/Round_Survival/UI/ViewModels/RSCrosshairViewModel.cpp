// Round Survival

#include "UI/ViewModels/RSCrosshairViewModel.h"

void URSCrosshairViewModel::SetAimingAtEnemy(bool bInAimingAtEnemy)
{
	UE_MVVM_SET_PROPERTY_VALUE(bAimingAtEnemy, bInAimingAtEnemy);
}
