// Round Survival

#include "PowerUps/RSPowerUpDirector.h"

#include "Core/RSGameMode.h"
#include "Core/RSGameState.h"
#include "Core/RSPlayerState.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "PowerUps/RSPowerUpDefinition.h"
#include "PowerUps/RSPowerUpPickup.h"
#include "PowerUps/RSPowerUpSettings.h"
#include "RSLogChannels.h"
#include "Rounds/RSRoundDirector.h"

void URSPowerUpDirector::Initialise(ARSGameMode* InGameMode, URSRoundDirector* InRoundDirector, const URSPowerUpSettings* InSettings)
{
	GameMode = InGameMode;
	RoundDirector = InRoundDirector;
	Settings = InSettings;

	if (InRoundDirector)
	{
		EnemyKilledHandle = InRoundDirector->OnEnemyKilled.AddUObject(this, &ThisClass::HandleEnemyKilled);
	}

	if (ARSGameState* GameState = InGameMode ? InGameMode->GetGameState<ARSGameState>() : nullptr)
	{
		RoundNumberHandle = GameState->OnRoundNumberChanged.AddUObject(this, &ThisClass::HandleRoundNumberChanged);
	}
}

void URSPowerUpDirector::Shutdown()
{
	if (URSRoundDirector* Director = RoundDirector.Get())
	{
		Director->OnEnemyKilled.Remove(EnemyKilledHandle);
	}

	if (ARSGameState* GameState = GameMode.IsValid() ? GameMode->GetGameState<ARSGameState>() : nullptr)
	{
		GameState->OnRoundNumberChanged.Remove(RoundNumberHandle);
	}

	EnemyKilledHandle.Reset();
	RoundNumberHandle.Reset();
}

UWorld* URSPowerUpDirector::GetGameWorld() const
{
	return GameMode.IsValid() ? GameMode->GetWorld() : nullptr;
}

void URSPowerUpDirector::HandleRoundNumberChanged(int32 RoundNumber)
{
	DropsThisRound = 0;
}

void URSPowerUpDirector::HandleEnemyKilled(ARSZombieCharacter* Enemy, AActor* Killer, bool /*bHeadshot*/)
{
	// A player's kill is the one the scrap award pays: the killer arrives as their player state.
	if (!Settings || !Enemy || !Cast<ARSPlayerState>(Killer))
	{
		return;
	}

	if (KillsSinceLastDrop < MAX_int32)
	{
		++KillsSinceLastDrop;
	}

	if (DropsThisRound >= Settings->MaxDropsPerRound || KillsSinceLastDrop < Settings->MinKillsBetweenDrops
		|| FMath::FRand() >= Settings->DropChance)
	{
		return;
	}

	if (SpawnPowerUp(Settings->PickPowerUp(), Enemy->GetActorLocation()))
	{
		++DropsThisRound;
		KillsSinceLastDrop = 0;

		UE_LOG(LogRS, Log, TEXT("A kill dropped a power-up: %d of %d this round."), DropsThisRound, Settings->MaxDropsPerRound);
	}
}

ARSPowerUpPickup* URSPowerUpDirector::SpawnPowerUp(const URSPowerUpDefinition* PowerUp, const FVector& Location)
{
	UWorld* World = GetGameWorld();
	if (!World || !Settings || !PowerUp || !Settings->PickupClass)
	{
		return nullptr;
	}

	// On the floor the players walk on, not inside a wall or in the air where the body fell.
	FNavLocation OnFloor;
	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const FVector Extent(Settings->NavigationProjectionExtent);
	if (!NavSystem || !NavSystem->ProjectPointToNavigation(Location, OnFloor, Extent))
	{
		UE_LOG(LogRS, Log, TEXT("No floor under %s for [%s]; nothing dropped."), *Location.ToCompactString(), *GetNameSafe(PowerUp));
		return nullptr;
	}

	const FTransform Transform(FRotator::ZeroRotator, OnFloor.Location);
	ARSPowerUpPickup* Pickup = World->SpawnActorDeferred<ARSPowerUpPickup>(Settings->PickupClass, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return nullptr;
	}

	Pickup->Initialise(PowerUp, Settings->PickupLifetime, Settings->PickupBlinkSeconds);
	Pickup->FinishSpawning(Transform);

	UE_LOG(LogRS, Log, TEXT("[%s] dropped at %s."), *GetNameSafe(PowerUp), *OnFloor.Location.ToCompactString());

	return Pickup;
}
