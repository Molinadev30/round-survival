// Round Survival

#include "Core/RSGameMode.h"

#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "Core/RSGameState.h"
#include "Core/RSPlayerController.h"
#include "Core/RSPlayerState.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "PowerUps/RSPowerUpDirector.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "Rounds/RSRoundDirector.h"
#include "Rounds/RSRoundSettings.h"
#include "TimerManager.h"
#include "UI/RSHUD.h"

ARSGameMode::ARSGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = ARSGameState::StaticClass();
	PlayerStateClass = ARSPlayerState::StaticClass();
	PlayerControllerClass = ARSPlayerController::StaticClass();
	HUDClass = ARSHUD::StaticClass();

	// DefaultPawnClass is left at the engine default until the player character exists. It is
	// then set per-map in a Blueprint child, because the pawn carries meshes and animation
	// assets and a C++ default would hardcode a content path.
}

void ARSGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!RoundSettings)
	{
		UE_LOG(LogRSRounds, Log, TEXT("[%s] has no round settings; no round director on this map."), *GetNameSafe(this));
		return;
	}

	RoundDirector = NewObject<URSRoundDirector>(this);
	RoundDirector->Initialise(this, RoundSettings);

	// Like the power-up director below, before the match starts so the first kill is counted.
	EnemyKilledHandle = RoundDirector->OnEnemyKilled.AddUObject(this, &ThisClass::HandleEnemyKilled);

	// Before the match starts, so it hears the very first kill.
	if (PowerUpSettings)
	{
		PowerUpDirector = NewObject<URSPowerUpDirector>(this);
		PowerUpDirector->Initialise(this, RoundDirector, PowerUpSettings);
	}

	// Started from BeginPlay and not from the constructor, so the game state exists to be written
	// to. The first thing a match does is an intermission, which is also the window the players
	// arrive in.
	RoundDirector->StartMatch();
}

void ARSGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PowerUpDirector)
	{
		PowerUpDirector->Shutdown();
	}

	if (RoundDirector)
	{
		RoundDirector->OnEnemyKilled.Remove(EnemyKilledHandle);
		RoundDirector->StopMatch();
	}

	TArray<TWeakObjectPtr<ARSPlayerState>> Watched;
	WatchedPlayers.GetKeys(Watched);

	for (const TWeakObjectPtr<ARSPlayerState>& PlayerState : Watched)
	{
		UnwatchPlayer(PlayerState.Get());
	}

	WatchedPlayers.Reset();

	Super::EndPlay(EndPlayReason);
}

void ARSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	WatchPlayer(NewPlayer ? NewPlayer->GetPlayerState<ARSPlayerState>() : nullptr);
}

void ARSGameMode::Logout(AController* Exiting)
{
	UnwatchPlayer(Exiting ? Exiting->GetPlayerState<ARSPlayerState>() : nullptr);

	Super::Logout(Exiting);

	// A client leaving because the match is restarting may be the last one the server waits for.
	SwitchLevelOnceClientsLeft(Exiting);

	// Next frame, once the player state has left the list: the last player standing walking out
	// loses the match for whoever is still on the floor.
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::CheckForMatchLost);
}

void ARSGameMode::WatchPlayer(ARSPlayerState* PlayerState)
{
	URSAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetRSAbilitySystemComponent() : nullptr;
	if (!ASC || WatchedPlayers.Contains(PlayerState))
	{
		return;
	}

	FWatchedPlayer& Watched = WatchedPlayers.Add(PlayerState);
	Watched.AbilitySystem = ASC;

	Watched.DownedHandle = ASC->RegisterGameplayTagEvent(RSGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleDownedTagChanged, TWeakObjectPtr<ARSPlayerState>(PlayerState));
	Watched.DeadHandle = ASC->RegisterGameplayTagEvent(RSGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleDeadTagChanged, TWeakObjectPtr<ARSPlayerState>(PlayerState));
}

void ARSGameMode::UnwatchPlayer(ARSPlayerState* PlayerState)
{
	FWatchedPlayer Watched;
	if (!WatchedPlayers.RemoveAndCopyValue(PlayerState, Watched))
	{
		return;
	}

	if (URSAbilitySystemComponent* ASC = Watched.AbilitySystem.Get())
	{
		ASC->RegisterGameplayTagEvent(RSGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved).Remove(Watched.DownedHandle);
		ASC->RegisterGameplayTagEvent(RSGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(Watched.DeadHandle);
	}
}

void ARSGameMode::HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount, TWeakObjectPtr<ARSPlayerState> PlayerState)
{
	// NewOrRemoved fires once going down and once getting up; only the first is a down.
	if (NewCount > 0)
	{
		if (ARSPlayerState* RSPlayerState = PlayerState.Get())
		{
			RSPlayerState->RecordDown();
		}
	}

	CheckForMatchLost();
}

void ARSGameMode::HandleEnemyKilled(ARSZombieCharacter* Enemy, AActor* Killer, bool bHeadshot)
{
	// A player's kill arrives as their player state; a Purge or a debug wipe as the enemy itself.
	if (ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(Killer))
	{
		RSPlayerState->RecordKill(bHeadshot);
	}
}

void ARSGameMode::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount, TWeakObjectPtr<ARSPlayerState> PlayerState)
{
	if (NewCount > 0)
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::RetirePlayer, PlayerState));
	}

	CheckForMatchLost();
}

bool ARSGameMode::IsPlayerStanding(const ARSPlayerState* PlayerState)
{
	const URSAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetRSAbilitySystemComponent() : nullptr;

	// Second Wind's self-revive is put on before the downed effect, so the down that is about to be
	// undone never reads as a lost match.
	return ASC
		&& (!ASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed) || ASC->HasMatchingGameplayTag(RSGameplayTags::State_SelfReviving))
		&& !ASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead);
}

void ARSGameMode::CheckForMatchLost()
{
	// Only a match being played can be lost. WaitingToStart is also the instant a forced round
	// passes through.
	if (!RoundDirector || !GameState)
	{
		return;
	}

	const ERSRoundPhase Phase = RoundDirector->GetPhase();
	if (Phase == ERSRoundPhase::GameOver || Phase == ERSRoundPhase::WaitingToStart)
	{
		return;
	}

	bool bAnyPlayer = false;

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(PlayerState);
		if (!IsValid(RSPlayerState) || RSPlayerState->IsOnlyASpectator())
		{
			continue;
		}

		bAnyPlayer = true;

		if (IsPlayerStanding(RSPlayerState))
		{
			return;
		}
	}

	// One rule for every player count. Alone, going down leaves nobody standing, so it ends the
	// match on the spot — nobody is coming.
	if (bAnyPlayer)
	{
		RoundDirector->EnterGameOver();
	}
}

void ARSGameMode::RetirePlayer(TWeakObjectPtr<ARSPlayerState> PlayerState)
{
	ARSPlayerState* RSPlayerState = PlayerState.Get();
	const URSAbilitySystemComponent* ASC = RSPlayerState ? RSPlayerState->GetRSAbilitySystemComponent() : nullptr;

	// Brought back in the frame it waited, by a round ending at that very moment.
	if (!ASC || !ASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		return;
	}

	ARSPlayerController* Controller = Cast<ARSPlayerController>(RSPlayerState->GetOwningController());
	APawn* Pawn = RSPlayerState->GetPawn();

	// Teammates already watching this player have to be moved on to someone else once the body goes.
	// Left alone, the engine's camera would hold on to this player state and wait for its next pawn,
	// which a dead player does not get until the round is over.
	TArray<APlayerController*> Watchers;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Other = It->Get();
		if (Other && Other != Controller && Pawn && Other->GetViewTarget() == Pawn)
		{
			Watchers.Add(Other);
		}
	}

	// The body goes, and what it carried with it: the weapon slots and the equipment live on the
	// pawn. The ability system, and the scrap in it, live on the player state and stay.
	if (Controller)
	{
		Controller->UnPossess();
	}

	if (Pawn)
	{
		Pawn->Destroy();
	}

	if (Controller)
	{
		Controller->ChangeState(NAME_Spectating);
		Controller->ClientGotoState(NAME_Spectating);

		// The engine's spectator cycle, from the player after the one being watched; CanSpectate
		// decides who qualifies. The camera manager sends the choice on to the owning client.
		Controller->ViewAPlayer(+1);
	}

	for (APlayerController* Watcher : Watchers)
	{
		Watcher->ViewAPlayer(+1);
	}

	UE_LOG(LogRSRounds, Log, TEXT("[%s] died and is spectating until the round ends."), *GetNameSafe(RSPlayerState));
}

bool ARSGameMode::CanSpectate_Implementation(APlayerController* Viewer, APlayerState* ViewTarget)
{
	const ARSPlayerState* Target = Cast<ARSPlayerState>(ViewTarget);
	return Target && !Target->IsOnlyASpectator() && Super::CanSpectate_Implementation(Viewer, ViewTarget);
}

void ARSGameMode::RespawnDeadPlayers()
{
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(PlayerState);
		URSAbilitySystemComponent* ASC = RSPlayerState ? RSPlayerState->GetRSAbilitySystemComponent() : nullptr;
		AController* Controller = RSPlayerState ? RSPlayerState->GetOwningController() : nullptr;

		if (!ASC || !Controller || !ASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead))
		{
			continue;
		}

		// Alive again before there is a body, so the new pawn starts from a player who is not dead.
		ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(RSGameplayTags::State_Dead));

		if (RespawnEffect)
		{
			ASC->ApplyGameplayEffectToSelf(RespawnEffect->GetDefaultObject<UGameplayEffect>(), 1.0f, ASC->MakeEffectContext());
		}

		// A new pawn at a player start. Possessing it takes the controller out of spectating, and the
		// pawn gives itself the starting weapons.
		RestartPlayer(Controller);

		UE_LOG(LogRSRounds, Log, TEXT("[%s] respawned."), *GetNameSafe(RSPlayerState));
	}
}

void ARSGameMode::RequestRestartMatch(const APlayerController* Requester)
{
	// The host is the one player whose controller is local to the server.
	if (!Requester || !Requester->IsLocalController() || !RoundDirector || RoundDirector->GetPhase() != ERSRoundPhase::GameOver)
	{
		return;
	}

	RestartMatch();
}

void ARSGameMode::RestartMatch()
{
	UE_LOG(LogRSRounds, Log, TEXT("Restarting the match."));

	// The same map again, from round one; connected clients travel with the server. A non-seamless
	// travel rebuilds the world, so nothing from the lost match — the round, the enemies, anyone's
	// scrap — carries over.
	GetWorld()->ServerTravel(TEXT("?Restart"), /*bAbsolute=*/ false);

	// A listen server waits out the pause even with nobody connected; alone, there is nobody to wait for.
	SwitchLevelOnceClientsLeft();
}

void ARSGameMode::SwitchLevelOnceClientsLeft(const AController* Leaving)
{
	// Every controller leaves as the old world is torn down, the host's included; by then the wait is over.
	UWorld* World = GetWorld();
	if (!World || World->NextURL.IsEmpty() || World->NextSwitchCountdown <= 0.0f || (Leaving && Leaving->IsLocalController()))
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController && PlayerController != Leaving && !PlayerController->IsLocalController())
		{
			return;
		}
	}

	UE_LOG(LogRSRounds, Log, TEXT("Every client has left to travel: switching the map now."));
	World->NextSwitchCountdown = 0.0f;
}
