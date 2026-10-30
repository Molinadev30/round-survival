// Round Survival

#pragma once

#include "GameFramework/HUD.h"

#include "RSHUD.generated.h"

class UCommonActivatableWidget;
class URSRootLayout;

/**
 * Puts the HUD on screen and starts its viewmodels feeding.
 *
 * An AHUD exists only for a locally controlled player, so on a listen server the host gets one and
 * each client gets their own, and nothing here ever runs for a remote player. It draws nothing
 * itself: it adds the root layout, pushes the HUD onto its game layer, and binds the local
 * player's URSHUDSubsystem to this world for as long as it lives.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSHUD : public AHUD
{
	GENERATED_BODY()

protected:
	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	UPROPERTY(EditDefaultsOnly, Category = "RS|UI")
	TSubclassOf<URSRootLayout> RootLayoutClass;

	/** Pushed onto the root layout's game layer at start. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|UI")
	TSubclassOf<UCommonActivatableWidget> HUDLayoutClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<URSRootLayout> RootLayout;
};
