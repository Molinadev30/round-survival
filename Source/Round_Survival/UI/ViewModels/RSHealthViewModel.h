// Round Survival

#pragma once

#include "MVVMViewModelBase.h"

#include "RSHealthViewModel.generated.h"

/**
 * The health of the player on screen, as the damage vignette reads it: how much of it is missing, 0
 * at full health and 1 at none, and 0 when the screen is nobody's eyes. The moment of a hit is not
 * here — it is an event, not state — and reaches the vignette through
 * URSHUDSubsystem::OnViewedPlayerDamaged. Fed by URSHUDSubsystem from the health set of whoever the
 * camera says is on screen.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSHealthViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetMissingHealth(float InMissingHealth);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Health", meta = (AllowPrivateAccess = true))
	float MissingHealth = 0.0f;
};
