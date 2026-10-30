// Round Survival

#pragma once

#include "Camera/RSPlayerCameraManager.h"
#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "RSSpectatorViewModel.generated.h"

/**
 * The banner a dead player sees: who they are watching, and the keys that move the view on and
 * switch between the teammate's eyes and behind them.
 *
 * Holds text ready to bind and nothing else. Fed by URSHUDSubsystem from the camera manager, which
 * knows who is on screen and how; the keys are asked of Enhanced Input, so a rebinding shows.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSSpectatorViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Shows the banner on a teammate. The toggle names the view the key switches to, not the one in use. */
	void SetSpectating(const FText& InViewedPlayerName, ERSSpectatorView View, const FText& InNextKeyText, const FText& InToggleViewKeyText);

	/** Alive, or spectating with nobody to watch: no banner. */
	void ClearSpectating();

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Spectator", meta = (AllowPrivateAccess = true))
	ESlateVisibility BannerVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Spectator", meta = (AllowPrivateAccess = true))
	FText ViewedPlayerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Spectator", meta = (AllowPrivateAccess = true))
	FText NextKeyText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Spectator", meta = (AllowPrivateAccess = true))
	FText ToggleViewKeyText;

	/** "Third person" while watching through the eyes, "First person" from behind. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Spectator", meta = (AllowPrivateAccess = true))
	FText ToggleViewLabel;

	TOptional<ERSSpectatorView> CachedView;
};
