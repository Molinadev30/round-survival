// Round Survival

#include "UI/RSHUDLayout.h"

#include "Input/UIActionBindingHandle.h"

TOptional<FUIInputConfig> URSHUDLayout::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently);
}
