// Round Survival

#pragma once

#include "MVVMViewModelBase.h"

#include "RSScrapBoardViewModel.generated.h"

class URSPlayerScrapViewModel;

/**
 * Every player's scrap, as the genre shows it: one row per player in the match.
 *
 * Entries is typed as plain objects because it binds straight onto a list view's
 * `SetListItems`, which takes objects; every entry is a URSPlayerScrapViewModel, and the
 * entry widget sets it on itself as its viewmodel.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSScrapBoardViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void AddEntry(URSPlayerScrapViewModel* Entry);
	void RemoveEntry(URSPlayerScrapViewModel* Entry);
	void ClearEntries();

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Scrap", meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UObject>> Entries;
};
