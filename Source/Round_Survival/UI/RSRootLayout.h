// Round Survival

#pragma once

#include "CommonUserWidget.h"

#include "RSRootLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

/**
 * The one widget ARSHUD adds to the screen. Everything else is pushed onto one of its layers.
 *
 * Lyra's primary game layout without the policy object and layer tags around it: two named
 * stacks, because a HUD and a menu over it are all this game has. A stack activates only its top
 * widget, and CommonUI takes the input mode from whatever is active — so pushing a pause menu onto
 * Menu frees the cursor, and popping it hands the game its input back, with no code doing either.
 *
 * The stacks are bound by name from the Blueprint's hierarchy. That is structure, not data: a
 * layout without both stacks is not a layout, and failing to compile says so earlier than a null
 * check at runtime would.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Pushes a widget onto the game layer: the HUD, and later anything that sits under menus. */
	UCommonActivatableWidget* PushToGameLayer(TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/** Pushes a widget onto the menu layer, above the game layer. */
	UCommonActivatableWidget* PushToMenuLayer(TSubclassOf<UCommonActivatableWidget> WidgetClass);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> GameLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> MenuLayer;
};
