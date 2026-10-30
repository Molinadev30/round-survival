// Round Survival

#pragma once

#include "CommonActivatableWidget.h"

#include "RSHUDLayout.generated.h"

/**
 * The in-game HUD, pushed onto the root layout's game layer.
 *
 * In C++ only for its input config: while the HUD is the active widget the game keeps all input
 * and the mouse stays captured. Everything it shows comes from viewmodels bound in its Blueprint.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSHUDLayout : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	//~UCommonActivatableWidget interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End of UCommonActivatableWidget interface
};
