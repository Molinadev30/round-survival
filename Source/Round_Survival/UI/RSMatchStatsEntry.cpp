// Round Survival

#include "UI/RSMatchStatsEntry.h"

#include "MVVMSubsystem.h"
#include "RSLogChannels.h"
#include "UI/ViewModels/RSPlayerMatchStatsViewModel.h"
#include "View/MVVMView.h"

void URSMatchStatsEntry::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	URSPlayerMatchStatsViewModel* ViewModel = Cast<URSPlayerMatchStatsViewModel>(ListItemObject);
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);

	if (!ViewModel || !View || !View->SetViewModelByClass(ViewModel))
	{
		UE_LOG(LogRS, Warning, TEXT("[%s] could not take %s as its match stats viewmodel."), *GetNameSafe(this), *GetNameSafe(ListItemObject));
	}
}
