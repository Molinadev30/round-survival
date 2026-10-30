// Round Survival

#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonUserWidget.h"

#include "RSMatchStatsEntry.generated.h"

/**
 * One row of the end-of-match board.
 *
 * The list view hands each row a URSPlayerMatchStatsViewModel as its list item, and this sets it on
 * the row's MVVM view — the row's Blueprint declares that viewmodel as Manual and binds it like any
 * other, as the scrap list's rows do. A row reused for another player simply gets that player's.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSMatchStatsEntry : public UCommonUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	//~IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End of IUserObjectListEntry interface
};
