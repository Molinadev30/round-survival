// Round Survival

#include "Core/RSPlayerController.h"

#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "Camera/RSPlayerCameraManager.h"
#include "Core/RSGameMode.h"
#include "Core/RSPlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Weapons/RSWeaponStateComponent.h"

ARSPlayerController::ARSPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerCameraManagerClass = ARSPlayerCameraManager::StaticClass();

	WeaponStateComponent = CreateDefaultSubobject<URSWeaponStateComponent>(TEXT("WeaponStateComponent"));
}

ARSPlayerState* ARSPlayerController::GetRSPlayerState() const
{
	return CastChecked<ARSPlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

URSAbilitySystemComponent* ARSPlayerController::GetRSAbilitySystemComponent() const
{
	const ARSPlayerState* RSPS = GetRSPlayerState();
	return RSPS ? RSPS->GetRSAbilitySystemComponent() : nullptr;
}

void ARSPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	OnPlayerStateChanged.Broadcast(PlayerState);
}

void ARSPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	// Here rather than in BeginPlay: this is the point at which a controller, on the server or on a
	// client, is known to have a local player to add input to.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (MatchMappingContext)
		{
			Subsystem->AddMappingContext(MatchMappingContext, MatchMappingPriority);
		}
	}
}

void ARSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (RestartMatchAction)
	{
		EnhancedInput->BindAction(RestartMatchAction, ETriggerEvent::Started, this, &ThisClass::Input_RestartMatch);
	}

	if (SpectateNextAction)
	{
		EnhancedInput->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &ThisClass::Input_SpectateNext);
	}

	if (ToggleSpectatorViewAction)
	{
		EnhancedInput->BindAction(ToggleSpectatorViewAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleSpectatorView);
	}
}

void ARSPlayerController::BeginSpectatingState()
{
	Super::BeginSpectatingState();

	// The camera and the input are the local player's; the server's copy of a remote controller has neither.
	if (!IsLocalController())
	{
		return;
	}

	if (ARSPlayerCameraManager* Camera = Cast<ARSPlayerCameraManager>(PlayerCameraManager))
	{
		Camera->BeginSpectating();
	}

	// The server says who to watch. Left on, the engine's own camera management would take the view
	// back to this controller the moment the lost pawn's replication lands on a client, which can be
	// after the server's choice has arrived.
	bAutoManageActiveCameraTarget = false;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (SpectatorMappingContext)
		{
			// Applied now rather than at the next rebuild: the spectator banner asks Enhanced Input
			// for these keys as soon as the view changes, and would otherwise find none.
			FModifyContextOptions Options;
			Options.bForceImmediately = true;
			Subsystem->AddMappingContext(SpectatorMappingContext, SpectatorMappingPriority, Options);
		}
	}
}

void ARSPlayerController::EndSpectatingState()
{
	if (IsLocalController())
	{
		bAutoManageActiveCameraTarget = GetDefault<APlayerController>(GetClass())->bAutoManageActiveCameraTarget;

		if (ARSPlayerCameraManager* Camera = Cast<ARSPlayerCameraManager>(PlayerCameraManager))
		{
			Camera->EndSpectating();
		}

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (SpectatorMappingContext)
			{
				Subsystem->RemoveMappingContext(SpectatorMappingContext);
			}
		}
	}

	Super::EndSpectatingState();
}

void ARSPlayerController::Input_RestartMatch()
{
	// Only the host's controller lives on the server; a client pressing it has nothing to ask.
	// The game mode checks the rest — that it is the host, and that the match is over.
	if (!CanRestartMatch())
	{
		return;
	}

	if (ARSGameMode* GameMode = GetWorld()->GetAuthGameMode<ARSGameMode>())
	{
		GameMode->RequestRestartMatch(this);
	}
}

void ARSPlayerController::Input_SpectateNext()
{
	// The engine's own request: the server checks the spectating state again and walks the player
	// array, asking the game mode's CanSpectate of each candidate.
	if (IsInState(NAME_Spectating))
	{
		ServerViewNextPlayer();
	}
}

void ARSPlayerController::Input_ToggleSpectatorView()
{
	// The camera is this player's own business: nothing to ask the server.
	if (ARSPlayerCameraManager* Camera = Cast<ARSPlayerCameraManager>(PlayerCameraManager))
	{
		Camera->ToggleSpectatorView();
	}
}

void ARSPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	// Buffered ability input is resolved here rather than on tick: PostProcessInput runs after
	// every binding for the frame has fired, so a press and a release that landed in the same
	// frame are both known before anything decides what to activate.
	if (URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent())
	{
		RSASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}
