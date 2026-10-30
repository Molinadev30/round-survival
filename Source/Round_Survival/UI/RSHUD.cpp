// Round Survival

#include "UI/RSHUD.h"

#include "CommonActivatableWidget.h"
#include "Core/RSPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "RSLogChannels.h"
#include "UI/RSHUDSubsystem.h"
#include "UI/RSRootLayout.h"

void ARSHUD::BeginPlay()
{
	Super::BeginPlay();

	ARSPlayerController* PC = Cast<ARSPlayerController>(GetOwningPlayerController());
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	// Feed the viewmodels before any widget asks for them, so the first frame shows real values.
	if (URSHUDSubsystem* HUDSubsystem = LocalPlayer->GetSubsystem<URSHUDSubsystem>())
	{
		HUDSubsystem->BindToPlayer(PC);
	}

	if (!RootLayoutClass)
	{
		UE_LOG(LogRS, Warning, TEXT("[%s] has no root layout class; no HUD on screen."), *GetNameSafe(this));
		return;
	}

	RootLayout = CreateWidget<URSRootLayout>(PC, RootLayoutClass);
	RootLayout->AddToPlayerScreen();
	RootLayout->PushToGameLayer(HUDLayoutClass);
}

void ARSHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RootLayout)
	{
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
	}

	const APlayerController* PC = GetOwningPlayerController();
	if (ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr)
	{
		if (URSHUDSubsystem* HUDSubsystem = LocalPlayer->GetSubsystem<URSHUDSubsystem>())
		{
			HUDSubsystem->UnbindFromPlayer();
		}
	}

	Super::EndPlay(EndPlayReason);
}
