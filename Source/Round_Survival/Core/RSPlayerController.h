// Round Survival

#pragma once

#include "GameFramework/PlayerController.h"

#include "RSPlayerController.generated.h"

class ARSPlayerState;
class UInputAction;
class UInputMappingContext;
class URSAbilitySystemComponent;
class URSWeaponStateComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FRSControllerPlayerStateChanged, APlayerState* /*PlayerState*/);

/**
 * Player controller.
 *
 * Almost as thin as it looks: the only thing it owns is the weapon state component, which holds
 * the shooter's own hit feedback between firing and the server confirming it — per-player, and it
 * has to survive the pawn. The ability system lives on the player state so
 * that scrap and attributes survive a pawn dying, and input binding lives on the character where
 * the avatar is.
 *
 * The only input here is what a player has without a body: restarting a lost match, which only the
 * host may do, and while dead and spectating, moving on to the next teammate and switching between
 * watching through their eyes and from behind them.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARSPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "RS|Player")
	ARSPlayerState* GetRSPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "RS|Player")
	URSAbilitySystemComponent* GetRSAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	URSWeaponStateComponent* GetWeaponStateComponent() const { return WeaponStateComponent; }

	/**
	 * This controller's player state arrived, on the owning client. The server assigns it before
	 * anything local exists to care; a client can receive it after its HUD is already up, and the
	 * HUD's scrap list needs it to tell which row is the local player's.
	 */
	FRSControllerPlayerStateChanged OnPlayerStateChanged;

	/** The spectator's actions, so the HUD can name the keys bound to them. */
	const UInputAction* GetSpectateNextAction() const { return SpectateNextAction; }
	const UInputAction* GetToggleSpectatorViewAction() const { return ToggleSpectatorViewAction; }

	/** The restart action, so the game over screen can name its key. */
	const UInputAction* GetRestartMatchAction() const { return RestartMatchAction; }

	/**
	 * Whether this player is the one who may restart a lost match: the host, whose controller is the
	 * one local to the server. The same rule Input_RestartMatch applies, so the screen never offers a
	 * key that does nothing.
	 */
	bool CanRestartMatch() const { return IsLocalController() && HasAuthority(); }

	//~APlayerController interface
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	virtual void OnRep_PlayerState() override;
	virtual void ReceivedPlayer() override;
	virtual void SetupInputComponent() override;
	//~End of APlayerController interface

protected:
	//~APlayerController interface
	virtual void BeginSpectatingState() override;
	virtual void EndSpectatingState() override;
	//~End of APlayerController interface

	/** Always on for a local player: restarting the match. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	TObjectPtr<UInputMappingContext> MatchMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	int32 MatchMappingPriority = 0;

	/**
	 * On while dead and spectating, above the pawn's own context, so the fire key moves the view on
	 * rather than reaching the fire action of a pawn that no longer exists.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	TObjectPtr<UInputMappingContext> SpectatorMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	int32 SpectatorMappingPriority = 1;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	TObjectPtr<UInputAction> RestartMatchAction;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	TObjectPtr<UInputAction> SpectateNextAction;

	/** Switches between watching through the teammate's eyes and from behind them. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	TObjectPtr<UInputAction> ToggleSpectatorViewAction;

	void Input_RestartMatch();
	void Input_SpectateNext();
	void Input_ToggleSpectatorView();

private:
	UPROPERTY(VisibleAnywhere, Category = "RS|Weapon")
	TObjectPtr<URSWeaponStateComponent> WeaponStateComponent;
};
