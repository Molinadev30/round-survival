// Round Survival

#pragma once

#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "RSGameOverViewModel.generated.h"

class URSPlayerMatchStatsViewModel;

/**
 * The game over screen: how far the match got, what each player did, and who can start the next one.
 *
 * Holds text ready to bind and nothing else. Fed by URSHUDSubsystem from the replicated round phase,
 * so every machine shows it at once, spectators included. Only the host can restart, so the host is
 * told the key and everyone else is told to wait; the key is asked of Enhanced Input, so a rebinding
 * shows.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSGameOverViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** RoundReached is the round the match was lost in: the genre counts it as survived. */
	void SetGameOver(int32 RoundReached, bool bCanRestart, const FText& InRestartKeyText);

	/** The match is still going: no screen. */
	void ClearGameOver();

	/** The board's rows, in the order they are shown. Notifies only when the list or its order changed. */
	void SetRows(const TArray<URSPlayerMatchStatsViewModel*>& InRows);

private:
	/**
	 * One URSPlayerMatchStatsViewModel per player, best first. Typed as plain objects because it binds
	 * straight onto a list view's `SetListItems`, as the scrap list does.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|GameOver", meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UObject>> Rows;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|GameOver", meta = (AllowPrivateAccess = true))
	ESlateVisibility ScreenVisibility = ESlateVisibility::Collapsed;

	/** "You survived N rounds". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|GameOver", meta = (AllowPrivateAccess = true))
	FText SurvivedText;

	/** "Press Enter to restart" on the host, "Waiting for the host to restart" everywhere else. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|GameOver", meta = (AllowPrivateAccess = true))
	FText RestartText;

	int32 CachedRoundReached = INDEX_NONE;
};
