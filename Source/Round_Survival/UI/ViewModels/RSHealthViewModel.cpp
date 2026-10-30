// Round Survival

#include "UI/ViewModels/RSHealthViewModel.h"

void URSHealthViewModel::SetMissingHealth(float InMissingHealth)
{
	UE_MVVM_SET_PROPERTY_VALUE(MissingHealth, FMath::Clamp(InMissingHealth, 0.0f, 1.0f));
}
