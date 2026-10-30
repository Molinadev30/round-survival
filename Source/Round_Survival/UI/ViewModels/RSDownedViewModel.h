// Round Survival

#pragma once

#include "Core/RSPlayerState.h"
#include "MVVMViewModelBase.h"

#include "RSDownedViewModel.generated.h"

/**
 * The bleed-out of the player on screen, as the downed status reads it: the clock, and what is
 * happening to it — running out, or held by a teammate reviving.
 *
 * Changes only when the clock does, never as it runs: the widget counts it down against the server's
 * time. Fed by URSHUDSubsystem from the player state of whoever is on screen through their eyes; an
 * inactive clock when nobody is, or they are standing.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSDownedViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetDowned(const FRSBleedOutClock& InClock, bool bBeingRevived);

	/** Standing, or nobody on screen: an inactive clock. */
	void ClearDowned();

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Downed", meta = (AllowPrivateAccess = true))
	FRSBleedOutClock BleedOutClock;

	/**
	 * "Bleeding out", "Being revived" while a teammate holds the clock, or "Getting back up" when the
	 * clock is Second Wind's.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Downed", meta = (AllowPrivateAccess = true))
	FText StatusLabel;

	enum class EStatus : uint8 { BleedingOut, BeingRevived, GettingUp };

	TOptional<EStatus> CachedStatus;
};
