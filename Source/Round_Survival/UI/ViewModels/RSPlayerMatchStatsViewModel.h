// Round Survival

#pragma once

#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "RSPlayerMatchStatsViewModel.generated.h"

struct FRSPlayerMatchStats;

/**
 * One row of the end-of-match board: a player's name, score, kills, headshots, downs and revives.
 *
 * One per player in the match, listed by URSGameOverViewModel and handed to each list entry widget
 * as its list item — so this one is set on the widget manually, never resolved.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Manual"))
class ROUND_SURVIVAL_API URSPlayerMatchStatsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetPlayerName(const FString& InPlayerName);
	void SetStats(int32 InScore, const FRSPlayerMatchStats& InStats);

	/** The row of the player looking at the board is marked, as on the scrap list. */
	void SetIsLocalPlayer(bool bInIsLocalPlayer);

	/** What the board sorts by: score first, kills to break a tie, as the genre does. */
	int32 GetScore() const { return CachedScore; }
	int32 GetKills() const { return CachedKills; }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Match Stats", meta = (AllowPrivateAccess = true))
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Match Stats", meta = (AllowPrivateAccess = true))
	FText ScoreText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Match Stats", meta = (AllowPrivateAccess = true))
	FText KillsText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Match Stats", meta = (AllowPrivateAccess = true))
	FText HeadshotsText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Match Stats", meta = (AllowPrivateAccess = true))
	FText DownsText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Match Stats", meta = (AllowPrivateAccess = true))
	FText RevivesText;

	/** The marker and the highlight behind the local player's row. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Match Stats", meta = (AllowPrivateAccess = true))
	ESlateVisibility LocalPlayerMarkerVisibility = ESlateVisibility::Hidden;

	/** Sets one number's text, only when the number changed. */
	void SetNumber(int32& Cached, int32 NewValue, FText& Text, const UE::FieldNotification::FFieldId& FieldId);

	/** What the texts were last built from. FText compares by identity, so rebuilding unchanged text would notify every binding. */
	FString CachedPlayerName;
	int32 CachedScore = INDEX_NONE;
	int32 CachedKills = INDEX_NONE;
	int32 CachedHeadshots = INDEX_NONE;
	int32 CachedDowns = INDEX_NONE;
	int32 CachedRevives = INDEX_NONE;
};
