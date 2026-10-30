// Round Survival

#pragma once

#include "View/MVVMViewModelContextResolver.h"

#include "RSViewModelResolver.generated.h"

/**
 * Hands a widget the HUD viewmodel of the type it asks for, from its owning player's URSHUDSubsystem.
 *
 * Set as the resolver on a widget's viewmodel entry, so a widget only declares which viewmodel it
 * wants and never fetches anything itself: any widget anywhere in the HUD — or a pause menu later —
 * gets the same instance its player's HUD is showing. The subsystem owns the viewmodels, so
 * nothing is destroyed when a widget lets go of one.
 */
UCLASS(DisplayName = "RS HUD Viewmodel Resolver")
class ROUND_SURVIVAL_API URSViewModelResolver : public UMVVMViewModelContextResolver
{
	GENERATED_BODY()

public:
	//~UMVVMViewModelContextResolver interface
	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const override;
	virtual void DestroyInstance(UObject* ViewModel, const UMVVMView* View) const override;
	//~End of UMVVMViewModelContextResolver interface
};
