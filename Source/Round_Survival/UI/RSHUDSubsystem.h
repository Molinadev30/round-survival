// Round Survival

#pragma once

#include "Core/RSPlayerState.h"
#include "GameplayEffectTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "RSHUDSubsystem.generated.h"

class AGameStateBase;
class APawn;
class APlayerState;
class ARSGameState;
class ARSPlayerCameraManager;
class UInputAction;
class ARSPlayerController;
class ARSPlayerState;
class UAbilitySystemComponent;
class URSCrosshairViewModel;
class URSDownedTeammatesViewModel;
class URSDownedTeammateViewModel;
class URSDownedViewModel;
class URSGameOverViewModel;
class URSHealthViewModel;
class URSInteractionViewModel;
class URSInteractorComponent;
class URSPlayerMatchStatsViewModel;
class URSPlayerScrapViewModel;
class URSPowerUpDefinition;
class URSPowerUpsViewModel;
class URSRoundViewModel;
class URSScrapBoardViewModel;
class URSSpectatorViewModel;
class URSWeaponItemInstance;
class URSWeaponSlotsComponent;
class URSWeaponViewModel;
enum class ERSRoundPhase : uint8;
struct FActiveGameplayEffect;
struct FGameplayEffectSpec;
struct FOnAttributeChangeData;

/** One player in the match, what the HUD shows of them, and what this subsystem listens to on them. */
USTRUCT()
struct FRSTrackedPlayer
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ARSPlayerState> PlayerState;

	/** Their row on the scrap list. */
	UPROPERTY()
	TObjectPtr<URSPlayerScrapViewModel> ScrapViewModel;

	/** Their marker, listed only while they are down. */
	UPROPERTY()
	TObjectPtr<URSDownedTeammateViewModel> DownedViewModel;

	/** Their row on the end-of-match board, kept up to date all match and listed once it is lost. */
	UPROPERTY()
	TObjectPtr<URSPlayerMatchStatsViewModel> MatchStatsViewModel;

	FDelegateHandle ScrapChangedHandle;
	FDelegateHandle MatchStatsHandle;
	FDelegateHandle NameChangedHandle;
	FDelegateHandle DownedTagHandle;
	FDelegateHandle BeingRevivedTagHandle;
	FDelegateHandle BleedOutClockHandle;
};

/**
 * Feeds the HUD's viewmodels. One per local player.
 *
 * The viewmodels hold text and nothing else; this is the only place that knows where each value
 * comes from. Everything arrives by delegate — the game state's round events, each player
 * state's scrap attribute and whether they are down, the health and bleed-out of the player on
 * screen, the team's timed power-ups on the local player's ability system, the pawn's weapon slots
 * and the drawn item's ammo stacks — and
 * nothing is read on tick — the interaction prompt included, which the interactor re-checks on its
 * own focus trace and reports only when its text changes.
 *
 * The hard part is order. On a client, the game state, the player states, the pawn and its
 * weapons replicate in whatever order the network delivers them, and any of them can arrive after
 * the HUD exists. So every source is bound when it shows up rather than assumed at start: the game
 * state through UWorld::GameStateSetEvent, players through the game state's player list, the pawn
 * through the controller's possession event, and the drawn item through the slots' change event.
 *
 * "The local player" and "the player on screen" are not always the same one. Dead and spectating,
 * the screen is a teammate's, and what the HUD shows of a body — the damage vignette, the bleed-out
 * — follows the
 * camera manager's answer to who is on screen and whether through their eyes, not the local player
 * state, whose health stays at zero until the round ends. What belongs to the local player whatever
 * they watch — their scrap, its popups — stays on the local player state.
 *
 * Bound by ARSHUD for as long as it lives, which is one world: the viewmodels outlive a map
 * change, the sources do not.
 */
UCLASS()
class ROUND_SURVIVAL_API URSHUDSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	//~USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End of USubsystem interface

	/** Starts feeding the viewmodels from this controller's world. Called by ARSHUD on BeginPlay. */
	void BindToPlayer(ARSPlayerController* InPlayerController);

	/** Lets go of every source. Called by ARSHUD on EndPlay. */
	void UnbindFromPlayer();

	/** The viewmodel of exactly this class, or null. What the viewmodel resolver hands to widgets. */
	UObject* FindViewModel(const UClass* ViewModelClass) const;

	/**
	 * The health of the player on screen just went down — the local player's own, or a teammate's
	 * watched through their eyes. A moment rather than state, so it is an event and not a viewmodel
	 * field: the damage vignette flashes on it. Regeneration never fires it, and neither does the
	 * view moving to a teammate with less health.
	 */
	FSimpleMulticastDelegate OnViewedPlayerDamaged;

	/**
	 * The local player's scrap moved by Delta, for Reason (an RS.Economy tag, or none): one award or
	 * one purchase. A moment, so an event like OnViewedPlayerDamaged; the scrap popup listens.
	 */
	FRSScrapDeltaReceived OnLocalScrapDelta;

private:
	// -- Game state: round and player list -----------------------------------------------------

	void HandleGameStateSet(AGameStateBase* GameState);
	void BindGameState(ARSGameState* InGameState);
	void UnbindGameState();
	void RefreshRound();
	void HandleRoundNumberChanged(int32 RoundNumber);
	void HandleRoundPhaseChanged(ERSRoundPhase Phase);

	/** Whether the match is lost. The game over screen then has the screen to itself. */
	bool IsGameOver() const;

	/** Everything the phase decides: the game over screen, and what it hides while it is up. */
	void RefreshPhase();

	// -- Players: the scrap list and downed teammates --------------------------------------------

	void HandlePlayerStateAdded(APlayerState* PlayerState);
	void HandlePlayerStateRemoved(APlayerState* PlayerState);
	void UntrackPlayer(int32 Index);
	void RefreshLocalPlayerMarkers();

	/** Lists every tracked player who is down, but the local player and whoever is seen through their eyes. */
	void RefreshDownedTeammates();

	/**
	 * Lists every tracked player on the game over board, best score first and most kills on a tie,
	 * once the match is lost; nobody before. Asked again whenever a row's numbers change, since on a
	 * client a player's last stats can land after the phase does.
	 */
	void RefreshMatchStatsBoard();

	void HandleLocalPlayerStateChanged(APlayerState* PlayerState);

	// -- The player on screen: health, bleed-out and the spectator banner ------------------------

	void HandleViewChanged();
	void BindViewedPlayerName(ARSPlayerState* PlayerState);
	void RefreshSpectator();

	void BindHealth(APlayerState* PlayerState);
	void UnbindHealth();
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void RefreshHealth();

	void BindDowned(ARSPlayerState* PlayerState);
	void UnbindDowned();
	void RefreshDowned();

	// -- Local player's scrap popups -------------------------------------------------------------

	void BindScrapDeltas(APlayerState* PlayerState);
	void UnbindScrapDeltas();

	// -- Timed power-ups -------------------------------------------------------------------------

	/**
	 * Follows the timed power-ups on this player's ability system: the local player's, which every
	 * pickup reaches whatever state they are in, so what is listed is the team's.
	 */
	void BindPowerUps(APlayerState* PlayerState);
	void UnbindPowerUps();
	void HandleEffectAdded(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	void HandleEffectRemoved(const FActiveGameplayEffect& Effect);

	/** Lists every timed power-up on the bound ability system but Leaving, which is on its way out. */
	void RefreshPowerUps(FActiveGameplayEffectHandle Leaving = FActiveGameplayEffectHandle());

	/** The power-up that applied this effect, or null: the power-up is the effect context's source object. */
	static const URSPowerUpDefinition* GetPowerUp(const FGameplayEffectSpec& Spec);

	// -- Drawn weapon ----------------------------------------------------------------------------

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void BindPawn(APawn* Pawn);
	void UnbindPawn();
	void HandleWeaponSlotsChanged(URSWeaponSlotsComponent* Slots);
	void BindActiveItem(URSWeaponItemInstance* Item);
	void HandleItemStatsChanged(URSWeaponItemInstance* Item);

	// -- Interaction prompt ----------------------------------------------------------------------

	void HandleFocusPromptChanged(const FText& Prompt);

	UFUNCTION()
	void HandleHoldProgressChanged(float Progress);

	/** The key bound to interact, as the prompt names it. */
	FText GetInteractKeyText() const;

	/** The first key bound to an action, as the HUD names it. Asked of Enhanced Input, so a rebinding shows. */
	FText GetKeyText(const UInputAction* Action) const;

	UPROPERTY(Transient)
	TObjectPtr<URSRoundViewModel> RoundViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSWeaponViewModel> WeaponViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSScrapBoardViewModel> ScrapBoardViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSInteractionViewModel> InteractionViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSCrosshairViewModel> CrosshairViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSHealthViewModel> HealthViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSSpectatorViewModel> SpectatorViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSDownedViewModel> DownedViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSDownedTeammatesViewModel> DownedTeammatesViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSGameOverViewModel> GameOverViewModel;

	UPROPERTY(Transient)
	TObjectPtr<URSPowerUpsViewModel> PowerUpsViewModel;

	TWeakObjectPtr<ARSPlayerController> PlayerController;
	FDelegateHandle PlayerStateChangedHandle;
	FDelegateHandle AimingAtEnemyHandle;

	TWeakObjectPtr<ARSPlayerCameraManager> Camera;
	FDelegateHandle ViewChangedHandle;

	TWeakObjectPtr<ARSPlayerState> ViewedPlayerState;
	FDelegateHandle ViewedNameHandle;

	TWeakObjectPtr<UWorld> BoundWorld;
	FDelegateHandle GameStateSetHandle;

	TWeakObjectPtr<ARSGameState> GameState;
	FDelegateHandle RoundNumberHandle;
	FDelegateHandle RoundPhaseHandle;
	FDelegateHandle PlayerAddedHandle;
	FDelegateHandle PlayerRemovedHandle;

	UPROPERTY(Transient)
	TArray<FRSTrackedPlayer> TrackedPlayers;

	TWeakObjectPtr<UAbilitySystemComponent> HealthASC;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;

	TWeakObjectPtr<ARSPlayerState> DownedSource;
	FDelegateHandle BleedOutClockHandle;
	FDelegateHandle BeingRevivedHandle;

	TWeakObjectPtr<ARSPlayerState> ScrapDeltaSource;
	FDelegateHandle ScrapDeltaHandle;

	TWeakObjectPtr<UAbilitySystemComponent> PowerUpASC;
	FDelegateHandle EffectAddedHandle;
	FDelegateHandle EffectRemovedHandle;

	/** Each timed power-up's time-change event: a second pickup restarts it without adding an effect. */
	TMap<FActiveGameplayEffectHandle, FDelegateHandle> PowerUpTimeHandles;

	TWeakObjectPtr<URSWeaponSlotsComponent> WeaponSlots;
	FDelegateHandle WeaponSlotsHandle;

	TWeakObjectPtr<URSWeaponItemInstance> ActiveItem;
	FDelegateHandle ItemStatsHandle;

	TWeakObjectPtr<URSInteractorComponent> Interactor;
	FDelegateHandle FocusPromptHandle;
};
