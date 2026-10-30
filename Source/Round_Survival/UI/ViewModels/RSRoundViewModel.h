// Round Survival

#pragma once

#include "Components/SlateWrapperTypes.h"
#include "Core/RSGameState.h"
#include "MVVMViewModelBase.h"

#include "RSRoundViewModel.generated.h"

/**
 * The round counter: which round, and what it is doing.
 *
 * Holds text ready to bind and nothing else. It does not know where the round comes from —
 * URSHUDSubsystem listens to the game state and calls SetRound.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSRoundViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Round 0 means no round has started — the weapon bench, or before the first intermission — and hides the counter. */
	void SetRound(int32 RoundNumber, ERSRoundPhase Phase);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Round", meta = (AllowPrivateAccess = true))
	FText RoundText;

	/** Empty while a round is being fought; the genre shows nothing then. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Round", meta = (AllowPrivateAccess = true))
	FText PhaseText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Round", meta = (AllowPrivateAccess = true))
	ESlateVisibility RoundVisibility = ESlateVisibility::Collapsed;

	int32 CachedRoundNumber = INDEX_NONE;
	TOptional<ERSRoundPhase> CachedPhase;
};
