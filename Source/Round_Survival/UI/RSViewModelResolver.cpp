// Round Survival

#include "UI/RSViewModelResolver.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "RSLogChannels.h"
#include "UI/RSHUDSubsystem.h"

UObject* URSViewModelResolver::CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const
{
	const ULocalPlayer* LocalPlayer = UserWidget ? UserWidget->GetOwningLocalPlayer() : nullptr;
	const URSHUDSubsystem* HUDSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<URSHUDSubsystem>() : nullptr;

	UObject* ViewModel = HUDSubsystem ? HUDSubsystem->FindViewModel(ExpectedType) : nullptr;

	UE_CLOG(!ViewModel, LogRS, Warning, TEXT("[%s] could not resolve a %s viewmodel: %s."),
		*GetNameSafe(UserWidget), *GetNameSafe(ExpectedType),
		!LocalPlayer ? TEXT("the widget has no owning local player") : TEXT("the HUD subsystem has no viewmodel of that class"));

	return ViewModel;
}

void URSViewModelResolver::DestroyInstance(UObject* /*ViewModel*/, const UMVVMView* /*View*/) const
{
	// Owned by the HUD subsystem, which outlives every widget that borrows them.
}
