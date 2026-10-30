// Round Survival

#include "UI/RSHUDSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/RSEconomySet.h"
#include "Algo/StableSort.h"
#include "AbilitySystem/Attributes/RSHealthSet.h"
#include "Camera/RSPlayerCameraManager.h"
#include "Core/RSGameState.h"
#include "Core/RSPlayerCharacter.h"
#include "Core/RSPlayerController.h"
#include "Core/RSPlayerState.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Input/RSInputConfig.h"
#include "Interaction/RSInteractorComponent.h"
#include "PowerUps/RSPowerUpDefinition.h"
#include "RSGameplayTags.h"
#include "UI/ViewModels/RSCrosshairViewModel.h"
#include "UI/ViewModels/RSDownedTeammateViewModel.h"
#include "UI/ViewModels/RSDownedTeammatesViewModel.h"
#include "UI/ViewModels/RSDownedViewModel.h"
#include "UI/ViewModels/RSGameOverViewModel.h"
#include "UI/ViewModels/RSHealthViewModel.h"
#include "UI/ViewModels/RSInteractionViewModel.h"
#include "UI/ViewModels/RSPlayerMatchStatsViewModel.h"
#include "UI/ViewModels/RSPlayerScrapViewModel.h"
#include "UI/ViewModels/RSPowerUpsViewModel.h"
#include "UI/ViewModels/RSRoundViewModel.h"
#include "UI/ViewModels/RSScrapBoardViewModel.h"
#include "UI/ViewModels/RSSpectatorViewModel.h"
#include "UI/ViewModels/RSWeaponViewModel.h"
#include "Weapons/RSWeaponItemInstance.h"
#include "Weapons/RSWeaponSlotsComponent.h"
#include "Weapons/RSWeaponStateComponent.h"

void URSHUDSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RoundViewModel = NewObject<URSRoundViewModel>(this);
	WeaponViewModel = NewObject<URSWeaponViewModel>(this);
	ScrapBoardViewModel = NewObject<URSScrapBoardViewModel>(this);
	InteractionViewModel = NewObject<URSInteractionViewModel>(this);
	CrosshairViewModel = NewObject<URSCrosshairViewModel>(this);
	HealthViewModel = NewObject<URSHealthViewModel>(this);
	SpectatorViewModel = NewObject<URSSpectatorViewModel>(this);
	DownedViewModel = NewObject<URSDownedViewModel>(this);
	DownedTeammatesViewModel = NewObject<URSDownedTeammatesViewModel>(this);
	GameOverViewModel = NewObject<URSGameOverViewModel>(this);
	PowerUpsViewModel = NewObject<URSPowerUpsViewModel>(this);
}

void URSHUDSubsystem::Deinitialize()
{
	UnbindFromPlayer();

	Super::Deinitialize();
}

UObject* URSHUDSubsystem::FindViewModel(const UClass* ViewModelClass) const
{
	for (UObject* ViewModel : { static_cast<UObject*>(RoundViewModel), static_cast<UObject*>(WeaponViewModel), static_cast<UObject*>(ScrapBoardViewModel), static_cast<UObject*>(InteractionViewModel), static_cast<UObject*>(CrosshairViewModel), static_cast<UObject*>(HealthViewModel), static_cast<UObject*>(SpectatorViewModel), static_cast<UObject*>(DownedViewModel), static_cast<UObject*>(DownedTeammatesViewModel), static_cast<UObject*>(GameOverViewModel), static_cast<UObject*>(PowerUpsViewModel) })
	{
		if (ViewModel && ViewModel->IsA(ViewModelClass))
		{
			return ViewModel;
		}
	}

	return nullptr;
}

void URSHUDSubsystem::BindToPlayer(ARSPlayerController* InPlayerController)
{
	UnbindFromPlayer();

	UWorld* World = InPlayerController ? InPlayerController->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	PlayerController = InPlayerController;
	BoundWorld = World;

	// Which scrap row is ours, and whose scrap the popups show, is only known once our own player
	// state has arrived.
	PlayerStateChangedHandle = InPlayerController->OnPlayerStateChanged.AddUObject(this, &ThisClass::HandleLocalPlayerStateChanged);
	BindScrapDeltas(InPlayerController->PlayerState);
	BindPowerUps(InPlayerController->PlayerState);

	// Whose health the vignette shows, and whether there is a spectator banner, is the camera's
	// answer to who is on screen. The camera manager is spawned with the controller, so it is here.
	if (ARSPlayerCameraManager* RSCamera = Cast<ARSPlayerCameraManager>(InPlayerController->PlayerCameraManager))
	{
		Camera = RSCamera;
		ViewChangedHandle = RSCamera->OnViewChanged.AddUObject(this, &ThisClass::HandleViewChanged);
	}
	HandleViewChanged();

	// The game state may not have replicated yet on a client. The world says when it does.
	GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
	HandleGameStateSet(World->GetGameState());

	// The crosshair's enemy check lives on the controller, so it survives the pawn.
	if (URSWeaponStateComponent* WeaponState = InPlayerController->GetWeaponStateComponent())
	{
		AimingAtEnemyHandle = WeaponState->OnAimingAtEnemyChanged.AddUObject(CrosshairViewModel.Get(), &URSCrosshairViewModel::SetAimingAtEnemy);
		CrosshairViewModel->SetAimingAtEnemy(WeaponState->IsAimingAtEnemy());
	}

	// Same for the pawn: on a client the controller can exist before the pawn it possesses
	// arrives, and a respawn swaps it later.
	InPlayerController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	BindPawn(InPlayerController->GetPawn());
}

void URSHUDSubsystem::UnbindFromPlayer()
{
	UnbindPawn();
	UnbindGameState();
	UnbindHealth();
	UnbindDowned();
	UnbindScrapDeltas();
	UnbindPowerUps();
	BindViewedPlayerName(nullptr);

	if (ARSPlayerCameraManager* OldCamera = Camera.Get())
	{
		OldCamera->OnViewChanged.Remove(ViewChangedHandle);
	}
	Camera.Reset();
	ViewChangedHandle.Reset();

	if (SpectatorViewModel)
	{
		SpectatorViewModel->ClearSpectating();
	}

	if (UWorld* World = BoundWorld.Get())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	GameStateSetHandle.Reset();
	BoundWorld.Reset();

	if (ARSPlayerController* PC = PlayerController.Get())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		PC->OnPlayerStateChanged.Remove(PlayerStateChangedHandle);

		if (URSWeaponStateComponent* WeaponState = PC->GetWeaponStateComponent())
		{
			WeaponState->OnAimingAtEnemyChanged.Remove(AimingAtEnemyHandle);
		}
	}
	AimingAtEnemyHandle.Reset();

	if (CrosshairViewModel)
	{
		CrosshairViewModel->SetAimingAtEnemy(false);
	}
	PlayerStateChangedHandle.Reset();
	PlayerController.Reset();
}

// -- Game state: round and player list -----------------------------------------------------------

void URSHUDSubsystem::HandleGameStateSet(AGameStateBase* InGameState)
{
	if (ARSGameState* RSGameState = Cast<ARSGameState>(InGameState))
	{
		BindGameState(RSGameState);
	}
}

void URSHUDSubsystem::BindGameState(ARSGameState* InGameState)
{
	if (GameState.Get() == InGameState)
	{
		return;
	}

	UnbindGameState();
	GameState = InGameState;

	RoundNumberHandle = InGameState->OnRoundNumberChanged.AddUObject(this, &ThisClass::HandleRoundNumberChanged);
	RoundPhaseHandle = InGameState->OnRoundPhaseChanged.AddUObject(this, &ThisClass::HandleRoundPhaseChanged);
	PlayerAddedHandle = InGameState->OnPlayerStateAdded.AddUObject(this, &ThisClass::HandlePlayerStateAdded);
	PlayerRemovedHandle = InGameState->OnPlayerStateRemoved.AddUObject(this, &ThisClass::HandlePlayerStateRemoved);

	RefreshRound();

	// Whoever is already listed; the delegates bring the rest.
	for (APlayerState* PlayerState : InGameState->PlayerArray)
	{
		HandlePlayerStateAdded(PlayerState);
	}

	RefreshPhase();
}

void URSHUDSubsystem::UnbindGameState()
{
	if (ARSGameState* OldGameState = GameState.Get())
	{
		OldGameState->OnRoundNumberChanged.Remove(RoundNumberHandle);
		OldGameState->OnRoundPhaseChanged.Remove(RoundPhaseHandle);
		OldGameState->OnPlayerStateAdded.Remove(PlayerAddedHandle);
		OldGameState->OnPlayerStateRemoved.Remove(PlayerRemovedHandle);
	}
	GameState.Reset();

	while (!TrackedPlayers.IsEmpty())
	{
		UntrackPlayer(TrackedPlayers.Num() - 1);
	}

	if (RoundViewModel)
	{
		RoundViewModel->SetRound(0, ERSRoundPhase::WaitingToStart);
	}

	if (GameOverViewModel)
	{
		GameOverViewModel->ClearGameOver();
	}
}

void URSHUDSubsystem::RefreshRound()
{
	const ARSGameState* RSGameState = GameState.Get();
	if (!RSGameState)
	{
		return;
	}

	RoundViewModel->SetRound(RSGameState->GetRoundNumber(), RSGameState->GetRoundPhase());

	// The round on the game over screen is read here too: round and phase replicate as separate
	// properties, and either can land second.
	const ARSPlayerController* PC = PlayerController.Get();
	if (IsGameOver() && PC)
	{
		GameOverViewModel->SetGameOver(RSGameState->GetRoundNumber(), PC->CanRestartMatch(), GetKeyText(PC->GetRestartMatchAction()));
	}
	else
	{
		GameOverViewModel->ClearGameOver();
	}
}

void URSHUDSubsystem::HandleRoundNumberChanged(int32 /*RoundNumber*/)
{
	RefreshRound();
}

void URSHUDSubsystem::HandleRoundPhaseChanged(ERSRoundPhase /*Phase*/)
{
	RefreshPhase();
}

bool URSHUDSubsystem::IsGameOver() const
{
	const ARSGameState* RSGameState = GameState.Get();
	return RSGameState && RSGameState->GetRoundPhase() == ERSRoundPhase::GameOver;
}

void URSHUDSubsystem::RefreshPhase()
{
	RefreshRound();

	// What the game over screen would sit on top of. Nobody dies once the match is lost, so the
	// downed stay down with a clock that no longer runs, and a spectator has nothing left to follow:
	// a countdown, markers or a banner over the screen would only contradict it.
	RefreshSpectator();
	RefreshDowned();
	RefreshDownedTeammates();
	RefreshPowerUps();
	RefreshMatchStatsBoard();
}

// -- Players: the scrap list and downed teammates ------------------------------------------------

void URSHUDSubsystem::HandlePlayerStateAdded(APlayerState* PlayerState)
{
	ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(PlayerState);
	const URSEconomySet* EconomySet = RSPlayerState ? RSPlayerState->GetEconomySet() : nullptr;
	UAbilitySystemComponent* ASC = RSPlayerState ? RSPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!EconomySet || !ASC)
	{
		return;
	}

	if (TrackedPlayers.ContainsByPredicate([RSPlayerState](const FRSTrackedPlayer& Tracked) { return Tracked.PlayerState == RSPlayerState; }))
	{
		return;
	}

	URSPlayerScrapViewModel* ViewModel = NewObject<URSPlayerScrapViewModel>(this);
	ViewModel->SetPlayerName(RSPlayerState->GetPlayerName());
	ViewModel->SetScrap(EconomySet->GetScrap());

	URSDownedTeammateViewModel* DownedTeammate = NewObject<URSDownedTeammateViewModel>(this);
	DownedTeammate->SetPlayerState(RSPlayerState);
	DownedTeammate->SetPlayerName(RSPlayerState->GetPlayerName());
	DownedTeammate->SetDowned(RSPlayerState->GetBleedOutClock(), ASC->HasMatchingGameplayTag(RSGameplayTags::State_BeingRevived));

	URSPlayerMatchStatsViewModel* MatchStats = NewObject<URSPlayerMatchStatsViewModel>(this);
	MatchStats->SetPlayerName(RSPlayerState->GetPlayerName());
	MatchStats->SetStats(RSPlayerState->GetMatchScore(), RSPlayerState->GetMatchStats());

	FRSTrackedPlayer& Tracked = TrackedPlayers.AddDefaulted_GetRef();
	Tracked.PlayerState = RSPlayerState;
	Tracked.ScrapViewModel = ViewModel;
	Tracked.DownedViewModel = DownedTeammate;
	Tracked.MatchStatsViewModel = MatchStats;

	// Fires on every machine, from replication on clients, so the board is right whichever of the
	// stats and the game over arrives first.
	Tracked.MatchStatsHandle = RSPlayerState->OnMatchStatsChanged.AddWeakLambda(MatchStats,
		[this, MatchStats](ARSPlayerState* ChangedPlayerState)
		{
			MatchStats->SetStats(ChangedPlayerState->GetMatchScore(), ChangedPlayerState->GetMatchStats());
			RefreshMatchStatsBoard();
		});

	// The ability system's own change delegate rather than the set's OnScrapChanged: on the server
	// that one only fires from an executed effect, and the balance also moves by other routes (a
	// direct attribute write, a duration effect). This fires for every change on every machine —
	// on clients from the replicated value, and scrap replicates to all of them.
	Tracked.ScrapChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(URSEconomySet::GetScrapAttribute()).AddWeakLambda(ViewModel,
		[ViewModel](const FOnAttributeChangeData& ChangeData)
		{
			ViewModel->SetScrap(ChangeData.NewValue);
		});

	Tracked.NameChangedHandle = RSPlayerState->OnPlayerNameChanged.AddWeakLambda(ViewModel,
		[ViewModel, DownedTeammate, MatchStats](ARSPlayerState* ChangedPlayerState)
		{
			ViewModel->SetPlayerName(ChangedPlayerState->GetPlayerName());
			DownedTeammate->SetPlayerName(ChangedPlayerState->GetPlayerName());
			MatchStats->SetPlayerName(ChangedPlayerState->GetPlayerName());
		});

	// Who is down is the tag, which replicates to everyone with the effect that grants it; how long
	// they have and whether someone is reviving them are the published clock and the revive's tag.
	Tracked.DownedTagHandle = ASC->RegisterGameplayTagEvent(RSGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved).AddWeakLambda(this,
		[this](const FGameplayTag, int32) { RefreshDownedTeammates(); });

	auto RefreshTeammate = [DownedTeammate, WeakPlayerState = TWeakObjectPtr<ARSPlayerState>(RSPlayerState)]()
	{
		const ARSPlayerState* TeammateState = WeakPlayerState.Get();
		const UAbilitySystemComponent* TeammateASC = TeammateState ? TeammateState->GetAbilitySystemComponent() : nullptr;
		if (TeammateASC)
		{
			DownedTeammate->SetDowned(TeammateState->GetBleedOutClock(), TeammateASC->HasMatchingGameplayTag(RSGameplayTags::State_BeingRevived));
		}
	};
	Tracked.BleedOutClockHandle = RSPlayerState->OnBleedOutClockChanged.AddWeakLambda(DownedTeammate, [RefreshTeammate](ARSPlayerState*) { RefreshTeammate(); });
	Tracked.BeingRevivedTagHandle = ASC->RegisterGameplayTagEvent(RSGameplayTags::State_BeingRevived, EGameplayTagEventType::NewOrRemoved).AddWeakLambda(DownedTeammate,
		[RefreshTeammate](const FGameplayTag, int32) { RefreshTeammate(); });

	ScrapBoardViewModel->AddEntry(ViewModel);
	RefreshLocalPlayerMarkers();
	RefreshDownedTeammates();
	RefreshMatchStatsBoard();
}

void URSHUDSubsystem::RefreshLocalPlayerMarkers()
{
	// Compared against the controller's player state rather than the player state's owner: on a
	// client a player state is added to the game state's list as it spawns, before its owner has
	// replicated, so asking the row "are you ours?" at that moment says no for every row. The
	// controller's own reference arrives through OnPlayerStateChanged, whichever comes first.
	const APlayerState* LocalPlayerState = PlayerController.IsValid() ? PlayerController->PlayerState.Get() : nullptr;

	for (const FRSTrackedPlayer& Tracked : TrackedPlayers)
	{
		const bool bIsLocalPlayer = LocalPlayerState && Tracked.PlayerState.Get() == LocalPlayerState;
		Tracked.ScrapViewModel->SetIsLocalPlayer(bIsLocalPlayer);
		Tracked.MatchStatsViewModel->SetIsLocalPlayer(bIsLocalPlayer);
	}
}

void URSHUDSubsystem::RefreshMatchStatsBoard()
{
	if (!GameOverViewModel)
	{
		return;
	}

	TArray<URSPlayerMatchStatsViewModel*> Rows;

	if (IsGameOver())
	{
		for (const FRSTrackedPlayer& Tracked : TrackedPlayers)
		{
			Rows.Add(Tracked.MatchStatsViewModel);
		}

		// Stable, so two players level on both keep the order they joined in.
		Algo::StableSort(Rows, [](const URSPlayerMatchStatsViewModel* A, const URSPlayerMatchStatsViewModel* B)
		{
			return A->GetScore() != B->GetScore() ? A->GetScore() > B->GetScore() : A->GetKills() > B->GetKills();
		});
	}

	GameOverViewModel->SetRows(Rows);
}

void URSHUDSubsystem::RefreshDownedTeammates()
{
	if (!DownedTeammatesViewModel)
	{
		return;
	}

	// Not the local player: their own countdown is on screen while it is their eyes, and while it is
	// not they are dead or watching from behind someone else. Not whoever's eyes the screen is either,
	// for the same reason — a spectator watching a downed teammate in first person reads that
	// teammate's countdown, and a marker there would sit inside the camera.
	const APlayerState* LocalPlayerState = PlayerController.IsValid() ? PlayerController->PlayerState.Get() : nullptr;
	const ARSPlayerState* ThroughEyes = DownedSource.Get();

	TArray<TObjectPtr<URSDownedTeammateViewModel>> Downed;

	// None at all once the match is lost: the game over screen has the screen to itself.
	if (IsGameOver())
	{
		DownedTeammatesViewModel->SetTeammates(Downed);
		return;
	}

	for (const FRSTrackedPlayer& Tracked : TrackedPlayers)
	{
		const ARSPlayerState* TrackedState = Tracked.PlayerState.Get();
		const UAbilitySystemComponent* ASC = TrackedState ? TrackedState->GetAbilitySystemComponent() : nullptr;

		if (ASC && TrackedState != LocalPlayerState && TrackedState != ThroughEyes && ASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed))
		{
			Downed.Add(Tracked.DownedViewModel);
		}
	}

	DownedTeammatesViewModel->SetTeammates(Downed);
}

void URSHUDSubsystem::HandlePlayerStateRemoved(APlayerState* PlayerState)
{
	const int32 Index = TrackedPlayers.IndexOfByPredicate([PlayerState](const FRSTrackedPlayer& Tracked) { return Tracked.PlayerState == PlayerState; });
	if (Index != INDEX_NONE)
	{
		UntrackPlayer(Index);
	}
}

void URSHUDSubsystem::UntrackPlayer(int32 Index)
{
	FRSTrackedPlayer& Tracked = TrackedPlayers[Index];

	if (ARSPlayerState* RSPlayerState = Tracked.PlayerState.Get())
	{
		if (UAbilitySystemComponent* ASC = RSPlayerState->GetAbilitySystemComponent())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(URSEconomySet::GetScrapAttribute()).Remove(Tracked.ScrapChangedHandle);
			ASC->RegisterGameplayTagEvent(RSGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved).Remove(Tracked.DownedTagHandle);
			ASC->RegisterGameplayTagEvent(RSGameplayTags::State_BeingRevived, EGameplayTagEventType::NewOrRemoved).Remove(Tracked.BeingRevivedTagHandle);
		}
		RSPlayerState->OnPlayerNameChanged.Remove(Tracked.NameChangedHandle);
		RSPlayerState->OnBleedOutClockChanged.Remove(Tracked.BleedOutClockHandle);
		RSPlayerState->OnMatchStatsChanged.Remove(Tracked.MatchStatsHandle);
	}

	ScrapBoardViewModel->RemoveEntry(Tracked.ScrapViewModel);
	TrackedPlayers.RemoveAt(Index);
	RefreshDownedTeammates();
	RefreshMatchStatsBoard();
}

void URSHUDSubsystem::HandleLocalPlayerStateChanged(APlayerState* PlayerState)
{
	RefreshLocalPlayerMarkers();
	RefreshDownedTeammates();
	BindScrapDeltas(PlayerState);
	BindPowerUps(PlayerState);
}

// -- The player on screen: health, bleed-out and the spectator banner ----------------------------

void URSHUDSubsystem::HandleViewChanged()
{
	const ARSPlayerCameraManager* RSCamera = Camera.Get();
	ARSPlayerState* OnScreen = RSCamera ? Cast<ARSPlayerState>(RSCamera->GetViewedPlayerState()) : nullptr;

	// The vignette and the bleed-out are what a body feels, so they show only through someone's eyes:
	// the local player's own, or a teammate's watched in first person. From behind a teammate, or
	// with nobody on screen, there is no body whose edges should redden or whose time is running out.
	ARSPlayerState* ThroughEyes = RSCamera && RSCamera->IsViewingThroughEyes() ? OnScreen : nullptr;
	BindHealth(ThroughEyes);
	BindDowned(ThroughEyes);
	RefreshDownedTeammates();

	// A banner only for a teammate. A client joining spends a moment in the engine's spectating
	// state looking at its own player before its pawn arrives, and that is not watching anyone.
	const APlayerState* LocalPlayerState = PlayerController.IsValid() ? PlayerController->PlayerState.Get() : nullptr;
	BindViewedPlayerName(RSCamera && RSCamera->IsSpectating() && OnScreen != LocalPlayerState ? OnScreen : nullptr);
	RefreshSpectator();
}

void URSHUDSubsystem::BindViewedPlayerName(ARSPlayerState* PlayerState)
{
	if (ViewedPlayerState.Get() == PlayerState)
	{
		return;
	}

	if (ARSPlayerState* OldPlayerState = ViewedPlayerState.Get())
	{
		OldPlayerState->OnPlayerNameChanged.Remove(ViewedNameHandle);
	}
	ViewedNameHandle.Reset();
	ViewedPlayerState = PlayerState;

	// The name replicates on its own and can land after the view does.
	if (PlayerState)
	{
		ViewedNameHandle = PlayerState->OnPlayerNameChanged.AddWeakLambda(this, [this](ARSPlayerState*) { RefreshSpectator(); });
	}
}

void URSHUDSubsystem::RefreshSpectator()
{
	const ARSPlayerCameraManager* RSCamera = Camera.Get();
	const ARSPlayerState* Watched = ViewedPlayerState.Get();
	const ARSPlayerController* PC = PlayerController.Get();

	if (!RSCamera || !Watched || !PC || IsGameOver())
	{
		SpectatorViewModel->ClearSpectating();
		return;
	}

	SpectatorViewModel->SetSpectating(FText::FromString(Watched->GetPlayerName()), RSCamera->GetSpectatorView(),
		GetKeyText(PC->GetSpectateNextAction()), GetKeyText(PC->GetToggleSpectatorViewAction()));
}

void URSHUDSubsystem::BindHealth(APlayerState* PlayerState)
{
	ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(PlayerState);
	UAbilitySystemComponent* ASC = RSPlayerState ? RSPlayerState->GetAbilitySystemComponent() : nullptr;
	if (HealthASC.Get() == ASC)
	{
		return;
	}

	UnbindHealth();

	if (!ASC || !RSPlayerState->GetHealthSet())
	{
		return;
	}

	HealthASC = ASC;

	// Health lives on the player state, so it outlives the pawn. Same delegate as the scrap list:
	// it fires on every machine and for every route a value can move by, and health replicates to
	// everyone, so a watched teammate's arrives like the local player's own.
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(URSHealthSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(URSHealthSet::GetMaxHealthAttribute()).AddWeakLambda(this,
		[this](const FOnAttributeChangeData&) { RefreshHealth(); });

	RefreshHealth();
}

void URSHUDSubsystem::UnbindHealth()
{
	if (UAbilitySystemComponent* ASC = HealthASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(URSHealthSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(URSHealthSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	}
	HealthASC.Reset();
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();

	if (HealthViewModel)
	{
		HealthViewModel->SetMissingHealth(0.0f);
	}
}

void URSHUDSubsystem::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();

	if (ChangeData.NewValue < ChangeData.OldValue)
	{
		OnViewedPlayerDamaged.Broadcast();
	}
}

void URSHUDSubsystem::RefreshHealth()
{
	const UAbilitySystemComponent* ASC = HealthASC.Get();
	if (!ASC)
	{
		return;
	}

	const float Health = ASC->GetNumericAttribute(URSHealthSet::GetHealthAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(URSHealthSet::GetMaxHealthAttribute());
	HealthViewModel->SetMissingHealth(MaxHealth > 0.0f ? 1.0f - Health / MaxHealth : 0.0f);
}

void URSHUDSubsystem::BindDowned(ARSPlayerState* PlayerState)
{
	if (DownedSource.Get() == PlayerState)
	{
		return;
	}

	UnbindDowned();

	UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	DownedSource = PlayerState;

	// The clock is published by the server on the player state; whether a teammate is holding it is
	// the revive's tag, which replicates to everyone on its own.
	BleedOutClockHandle = PlayerState->OnBleedOutClockChanged.AddWeakLambda(this, [this](ARSPlayerState*) { RefreshDowned(); });
	BeingRevivedHandle = ASC->RegisterGameplayTagEvent(RSGameplayTags::State_BeingRevived, EGameplayTagEventType::NewOrRemoved).AddWeakLambda(this,
		[this](const FGameplayTag, int32) { RefreshDowned(); });

	RefreshDowned();
}

void URSHUDSubsystem::UnbindDowned()
{
	if (ARSPlayerState* OldPlayerState = DownedSource.Get())
	{
		OldPlayerState->OnBleedOutClockChanged.Remove(BleedOutClockHandle);

		if (UAbilitySystemComponent* ASC = OldPlayerState->GetAbilitySystemComponent())
		{
			ASC->RegisterGameplayTagEvent(RSGameplayTags::State_BeingRevived, EGameplayTagEventType::NewOrRemoved).Remove(BeingRevivedHandle);
		}
	}
	DownedSource.Reset();
	BleedOutClockHandle.Reset();
	BeingRevivedHandle.Reset();

	if (DownedViewModel)
	{
		DownedViewModel->ClearDowned();
	}
}

void URSHUDSubsystem::RefreshDowned()
{
	const ARSPlayerState* PlayerState = DownedSource.Get();
	const UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	if (IsGameOver())
	{
		DownedViewModel->ClearDowned();
		return;
	}

	DownedViewModel->SetDowned(PlayerState->GetBleedOutClock(), ASC->HasMatchingGameplayTag(RSGameplayTags::State_BeingRevived));
}

// -- Local player's scrap popups -----------------------------------------------------------------

void URSHUDSubsystem::BindScrapDeltas(APlayerState* PlayerState)
{
	ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(PlayerState);
	if (ScrapDeltaSource.Get() == RSPlayerState)
	{
		return;
	}

	UnbindScrapDeltas();

	if (RSPlayerState)
	{
		ScrapDeltaSource = RSPlayerState;
		ScrapDeltaHandle = RSPlayerState->OnScrapDeltaReceived.AddWeakLambda(this,
			[this](int32 Delta, const FGameplayTag& Reason) { OnLocalScrapDelta.Broadcast(Delta, Reason); });
	}
}

void URSHUDSubsystem::UnbindScrapDeltas()
{
	if (ARSPlayerState* OldPlayerState = ScrapDeltaSource.Get())
	{
		OldPlayerState->OnScrapDeltaReceived.Remove(ScrapDeltaHandle);
	}
	ScrapDeltaSource.Reset();
	ScrapDeltaHandle.Reset();
}

// -- Timed power-ups -----------------------------------------------------------------------------

void URSHUDSubsystem::BindPowerUps(APlayerState* PlayerState)
{
	const ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(PlayerState);
	UAbilitySystemComponent* ASC = RSPlayerState ? RSPlayerState->GetAbilitySystemComponent() : nullptr;
	if (PowerUpASC.Get() == ASC)
	{
		return;
	}

	UnbindPowerUps();

	if (!ASC)
	{
		return;
	}

	PowerUpASC = ASC;

	// A timed power-up is a duration effect the server applies to every player. The player's ability
	// system replicates its effects whole to its owner, so the local player's copy is here with its
	// duration and the context naming the power-up, and these fire for it on the owning client as on
	// the server.
	EffectAddedHandle = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);
	EffectRemovedHandle = ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &ThisClass::HandleEffectRemoved);

	// Whatever was already running when the HUD bound.
	for (const FActiveGameplayEffect& Effect : &ASC->GetActiveGameplayEffects())
	{
		HandleEffectAdded(ASC, Effect.Spec, Effect.Handle);
	}

	RefreshPowerUps();
}

void URSHUDSubsystem::UnbindPowerUps()
{
	if (UAbilitySystemComponent* ASC = PowerUpASC.Get())
	{
		ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(EffectAddedHandle);
		ASC->OnAnyGameplayEffectRemovedDelegate().Remove(EffectRemovedHandle);

		for (const TPair<FActiveGameplayEffectHandle, FDelegateHandle>& TimeHandle : PowerUpTimeHandles)
		{
			if (FOnActiveGameplayEffectTimeChange* TimeChanged = ASC->OnGameplayEffectTimeChangeDelegate(TimeHandle.Key))
			{
				TimeChanged->Remove(TimeHandle.Value);
			}
		}
	}
	PowerUpASC.Reset();
	EffectAddedHandle.Reset();
	EffectRemovedHandle.Reset();
	PowerUpTimeHandles.Reset();

	if (PowerUpsViewModel)
	{
		PowerUpsViewModel->SetActivePowerUps({});
	}
}

void URSHUDSubsystem::HandleEffectAdded(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	if (!GetPowerUp(Spec) || PowerUpTimeHandles.Contains(Handle))
	{
		return;
	}

	// Picking the same power-up up again refreshes this effect's duration rather than adding one, and
	// says so only through the effect's own time-change event.
	if (FOnActiveGameplayEffectTimeChange* TimeChanged = ASC->OnGameplayEffectTimeChangeDelegate(Handle))
	{
		PowerUpTimeHandles.Add(Handle, TimeChanged->AddWeakLambda(this,
			[this](FActiveGameplayEffectHandle, float, float) { RefreshPowerUps(); }));
	}

	RefreshPowerUps();
}

void URSHUDSubsystem::HandleEffectRemoved(const FActiveGameplayEffect& Effect)
{
	// Its event set goes with it, binding and all.
	if (PowerUpTimeHandles.Remove(Effect.Handle) > 0)
	{
		RefreshPowerUps(Effect.Handle);
	}
}

void URSHUDSubsystem::RefreshPowerUps(FActiveGameplayEffectHandle Leaving)
{
	const UAbilitySystemComponent* ASC = PowerUpASC.Get();
	if (!PowerUpsViewModel || !ASC)
	{
		return;
	}

	TArray<FRSActivePowerUp> ActivePowerUps;

	// None once the match is lost: the game over screen has the screen to itself.
	if (!IsGameOver())
	{
		// The container keeps effects in the order they were applied, and a refresh does not move
		// one, so the entries stay where they are.
		for (const FActiveGameplayEffect& Effect : &ASC->GetActiveGameplayEffects())
		{
			const URSPowerUpDefinition* PowerUp = GetPowerUp(Effect.Spec);
			if (PowerUp && Effect.Handle != Leaving && !Effect.IsPendingRemove && Effect.GetDuration() > 0.0f)
			{
				FRSActivePowerUp& Entry = ActivePowerUps.AddDefaulted_GetRef();
				Entry.Color = PowerUp->Color;
				Entry.EndWorldTime = Effect.GetEndTime();
			}
		}
	}

	PowerUpsViewModel->SetActivePowerUps(ActivePowerUps);
}

const URSPowerUpDefinition* URSHUDSubsystem::GetPowerUp(const FGameplayEffectSpec& Spec)
{
	return Cast<URSPowerUpDefinition>(Spec.GetEffectContext().GetSourceObject());
}

// -- Drawn weapon --------------------------------------------------------------------------------

void URSHUDSubsystem::HandlePossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	BindPawn(NewPawn);
}

void URSHUDSubsystem::BindPawn(APawn* Pawn)
{
	UnbindPawn();

	if (URSInteractorComponent* PawnInteractor = Pawn ? Pawn->FindComponentByClass<URSInteractorComponent>() : nullptr)
	{
		Interactor = PawnInteractor;
		FocusPromptHandle = PawnInteractor->OnFocusPromptChanged.AddUObject(this, &ThisClass::HandleFocusPromptChanged);
		PawnInteractor->OnHoldProgressChanged.AddDynamic(this, &ThisClass::HandleHoldProgressChanged);
		HandleFocusPromptChanged(PawnInteractor->GetCurrentPrompt());
		HandleHoldProgressChanged(PawnInteractor->GetHoldProgress());
	}

	URSWeaponSlotsComponent* Slots = Pawn ? Pawn->FindComponentByClass<URSWeaponSlotsComponent>() : nullptr;
	if (!Slots)
	{
		return;
	}

	WeaponSlots = Slots;
	WeaponSlotsHandle = Slots->OnSlotsChanged.AddUObject(this, &ThisClass::HandleWeaponSlotsChanged);
	HandleWeaponSlotsChanged(Slots);
}

void URSHUDSubsystem::UnbindPawn()
{
	BindActiveItem(nullptr);

	if (URSInteractorComponent* OldInteractor = Interactor.Get())
	{
		OldInteractor->OnFocusPromptChanged.Remove(FocusPromptHandle);
		OldInteractor->OnHoldProgressChanged.RemoveDynamic(this, &ThisClass::HandleHoldProgressChanged);
	}
	Interactor.Reset();
	FocusPromptHandle.Reset();

	if (InteractionViewModel)
	{
		InteractionViewModel->SetPrompt(FText::GetEmpty(), FText::GetEmpty());
		InteractionViewModel->SetHoldProgress(0.0f);
	}

	if (URSWeaponSlotsComponent* Slots = WeaponSlots.Get())
	{
		Slots->OnSlotsChanged.Remove(WeaponSlotsHandle);
	}
	WeaponSlots.Reset();
	WeaponSlotsHandle.Reset();
}

void URSHUDSubsystem::HandleWeaponSlotsChanged(URSWeaponSlotsComponent* Slots)
{
	BindActiveItem(Slots ? Slots->GetActiveItem() : nullptr);
}

void URSHUDSubsystem::BindActiveItem(URSWeaponItemInstance* Item)
{
	if (ActiveItem.Get() != Item)
	{
		if (URSWeaponItemInstance* OldItem = ActiveItem.Get())
		{
			OldItem->OnStatsChanged.Remove(ItemStatsHandle);
		}
		ItemStatsHandle.Reset();

		ActiveItem = Item;

		if (Item)
		{
			ItemStatsHandle = Item->OnStatsChanged.AddUObject(this, &ThisClass::HandleItemStatsChanged);
		}
	}

	if (WeaponViewModel)
	{
		WeaponViewModel->SetItem(Item);
	}
}

void URSHUDSubsystem::HandleItemStatsChanged(URSWeaponItemInstance* Item)
{
	WeaponViewModel->SetItem(Item);
}

// -- Interaction prompt --------------------------------------------------------------------------

void URSHUDSubsystem::HandleFocusPromptChanged(const FText& Prompt)
{
	// The key is asked for whenever a prompt appears rather than once at bind time: the mapping
	// context is only added when the pawn restarts on the owning client, which can be after this
	// subsystem binds to it.
	InteractionViewModel->SetPrompt(Prompt, Prompt.IsEmpty() ? FText::GetEmpty() : GetInteractKeyText());
}

void URSHUDSubsystem::HandleHoldProgressChanged(float Progress)
{
	InteractionViewModel->SetHoldProgress(Progress);
}

FText URSHUDSubsystem::GetInteractKeyText() const
{
	const ARSPlayerController* PC = PlayerController.Get();
	const ARSPlayerCharacter* Character = PC ? Cast<ARSPlayerCharacter>(PC->GetPawn()) : nullptr;
	const URSInputConfig* InputConfig = Character ? Character->GetInputConfig() : nullptr;

	return GetKeyText(InputConfig ? InputConfig->FindAbilityInputActionForTag(RSGameplayTags::InputTag_Interact) : nullptr);
}

FText URSHUDSubsystem::GetKeyText(const UInputAction* Action) const
{
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Action || !InputSubsystem)
	{
		return FText::GetEmpty();
	}

	const TArray<FKey> Keys = InputSubsystem->QueryKeysMappedToAction(Action);
	if (Keys.IsEmpty())
	{
		return FText::GetEmpty();
	}

	return Keys[0].GetDisplayName(/*bLongDisplayName=*/ false);
}
