// Round Survival

#pragma once

#include "Core/RSPlayerState.h"
#include "MVVMViewModelBase.h"

#include "RSDownedTeammateViewModel.generated.h"

/**
 * One downed teammate, as the marker over them reads it: their name, their bleed-out and whether
 * someone is reviving them.
 *
 * One per player in the match, made by URSHUDSubsystem and listed by URSDownedTeammatesViewModel only
 * while that player is down. Set on its marker manually, never resolved. Where the marker goes is not
 * here — it moves every frame, with the camera — but the player it follows is, for the marker layer
 * to project.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Manual"))
class ROUND_SURVIVAL_API URSDownedTeammateViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetPlayerState(ARSPlayerState* InPlayerState) { PlayerState = InPlayerState; }

	/** The teammate the marker follows. Their pawn is where it goes. */
	ARSPlayerState* GetPlayerState() const { return PlayerState.Get(); }

	void SetPlayerName(const FString& InPlayerName);
	void SetDowned(const FRSBleedOutClock& InClock, bool bInBeingRevived);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Downed", meta = (AllowPrivateAccess = true))
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Downed", meta = (AllowPrivateAccess = true))
	FRSBleedOutClock BleedOutClock;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Downed", meta = (AllowPrivateAccess = true))
	bool bBeingRevived = false;

	TWeakObjectPtr<ARSPlayerState> PlayerState;

	/** What PlayerName was built from. FText compares by identity. */
	FString CachedPlayerName;
};
