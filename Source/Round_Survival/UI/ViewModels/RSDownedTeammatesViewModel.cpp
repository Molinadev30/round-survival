// Round Survival

#include "UI/ViewModels/RSDownedTeammatesViewModel.h"

#include "UI/ViewModels/RSDownedTeammateViewModel.h"

void URSDownedTeammatesViewModel::SetTeammates(const TArray<TObjectPtr<URSDownedTeammateViewModel>>& InTeammates)
{
	if (Teammates == InTeammates)
	{
		return;
	}

	Teammates = InTeammates;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Teammates);
}
