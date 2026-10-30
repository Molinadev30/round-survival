// Round Survival

#pragma once

#include "MVVMViewModelBase.h"

#include "RSCrosshairViewModel.generated.h"

/**
 * What the crosshair needs that is state rather than a per-frame value: whether its centre is on
 * a living enemy. The spread itself is read by URSCrosshairWidget every frame, since it changes
 * every frame. Fed by URSHUDSubsystem from the controller's URSWeaponStateComponent.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSCrosshairViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetAimingAtEnemy(bool bInAimingAtEnemy);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Crosshair", meta = (AllowPrivateAccess = true))
	bool bAimingAtEnemy = false;
};
