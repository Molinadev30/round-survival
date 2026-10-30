// Round Survival

#include "Core/RSGameState.h"

#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PowerUps/RSPowerUpDefinition.h"
#include "RSLogChannels.h"
#include "Sound/SoundBase.h"

ARSGameState::ARSGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Round counters change in bursts during a wave and the HUD reads them directly, so the
	// default game state update rate is too coarse.
	SetNetUpdateFrequency(30.0f);
}

void ARSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARSGameState, RoundNumber);
	DOREPLIFETIME(ARSGameState, RoundPhase);
	DOREPLIFETIME(ARSGameState, ZombiesAlive);
	DOREPLIFETIME(ARSGameState, ZombiesRemainingToSpawn);
	DOREPLIFETIME(ARSGameState, bPowerOn);
}

void ARSGameState::SetRoundNumber(int32 InRoundNumber)
{
	if (!HasAuthority() || RoundNumber == InRoundNumber)
	{
		return;
	}

	RoundNumber = InRoundNumber;

	// Broadcast locally as well: OnRep never runs on the authority, and on a listen server the
	// host's own HUD has to update like everyone else's.
	OnRoundNumberChanged.Broadcast(RoundNumber);
}

void ARSGameState::SetRoundPhase(ERSRoundPhase InPhase)
{
	if (!HasAuthority() || RoundPhase == InPhase)
	{
		return;
	}

	RoundPhase = InPhase;
	OnRoundPhaseChanged.Broadcast(RoundPhase);
}

void ARSGameState::SetZombieCounts(int32 InAlive, int32 InRemainingToSpawn)
{
	if (!HasAuthority() || (ZombiesAlive == InAlive && ZombiesRemainingToSpawn == InRemainingToSpawn))
	{
		return;
	}

	ZombiesAlive = InAlive;
	ZombiesRemainingToSpawn = InRemainingToSpawn;
	OnZombieCountChanged.Broadcast(ZombiesAlive, ZombiesRemainingToSpawn);
}

void ARSGameState::SetPowerOn(bool bInPowerOn)
{
	if (!HasAuthority() || bPowerOn == bInPowerOn)
	{
		return;
	}

	bPowerOn = bInPowerOn;

	UE_LOG(LogRS, Log, TEXT("The power is now %s."), bPowerOn ? TEXT("on") : TEXT("off"));

	OnPowerChanged.Broadcast(bPowerOn);
}

bool ARSGameState::IsSoloMatch() const
{
	int32 Players = 0;

	for (const APlayerState* PlayerState : PlayerArray)
	{
		if (IsValid(PlayerState) && !PlayerState->IsOnlyASpectator())
		{
			++Players;
		}
	}

	return Players == 1;
}

void ARSGameState::AddPlayerState(APlayerState* PlayerState)
{
	const bool bWasListed = PlayerArray.Contains(PlayerState);

	Super::AddPlayerState(PlayerState);

	if (!bWasListed && PlayerArray.Contains(PlayerState))
	{
		OnPlayerStateAdded.Broadcast(PlayerState);
	}
}

void ARSGameState::RemovePlayerState(APlayerState* PlayerState)
{
	const bool bWasListed = PlayerArray.Contains(PlayerState);

	Super::RemovePlayerState(PlayerState);

	if (bWasListed)
	{
		OnPlayerStateRemoved.Broadcast(PlayerState);
	}
}

void ARSGameState::MulticastPowerUpCollected_Implementation(const URSPowerUpDefinition* PowerUp)
{
	// Not placed in the world: it is the team being told, and the same wherever a player stands.
	if (GetNetMode() != NM_DedicatedServer && PowerUp && PowerUp->PickupSound)
	{
		UGameplayStatics::PlaySound2D(this, PowerUp->PickupSound);
	}
}

void ARSGameState::OnRep_RoundNumber()
{
	OnRoundNumberChanged.Broadcast(RoundNumber);
}

void ARSGameState::OnRep_RoundPhase()
{
	OnRoundPhaseChanged.Broadcast(RoundPhase);
}

void ARSGameState::OnRep_ZombieCounts()
{
	OnZombieCountChanged.Broadcast(ZombiesAlive, ZombiesRemainingToSpawn);
}

void ARSGameState::OnRep_PowerOn()
{
	OnPowerChanged.Broadcast(bPowerOn);
}
