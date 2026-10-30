// Round Survival

#include "UI/ViewModels/RSScrapBoardViewModel.h"

#include "UI/ViewModels/RSPlayerScrapViewModel.h"

void URSScrapBoardViewModel::AddEntry(URSPlayerScrapViewModel* Entry)
{
	if (Entry && !Entries.Contains(Entry))
	{
		Entries.Add(Entry);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
	}
}

void URSScrapBoardViewModel::RemoveEntry(URSPlayerScrapViewModel* Entry)
{
	if (Entries.Remove(Entry) > 0)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
	}
}

void URSScrapBoardViewModel::ClearEntries()
{
	if (!Entries.IsEmpty())
	{
		Entries.Reset();
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
	}
}
