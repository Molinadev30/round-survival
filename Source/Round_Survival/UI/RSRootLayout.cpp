// Round Survival

#include "UI/RSRootLayout.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UCommonActivatableWidget* URSRootLayout::PushToGameLayer(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	return WidgetClass ? GameLayer->AddWidget(WidgetClass) : nullptr;
}

UCommonActivatableWidget* URSRootLayout::PushToMenuLayer(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	return WidgetClass ? MenuLayer->AddWidget(WidgetClass) : nullptr;
}
