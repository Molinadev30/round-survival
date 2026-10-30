// Round Survival

#include "Rounds/RSRoundDirector.h"

#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "Core/RSGameMode.h"
#include "Core/RSGameState.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "Rounds/RSRoundSettings.h"
#include "Rounds/RSSpawnSubsystem.h"
#include "TimerManager.h"

namespace RSRoundDirector
{
	/** How often the debug horde queue tries to place another batch. */
	static constexpr float HordeReleaseInterval = 0.25f;
}

void URSRoundDirector::Initialise(ARSGameMode* InGameMode, URSRoundSettings* InSettings)
{
	GameMode = InGameMode;
	Settings = InSettings;

	// While the players are arriving, rather than in the first round's opening seconds.
	if (URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this); SpawnSubsystem && Settings)
	{
		SpawnSubsystem->PrewarmPool(Settings->GetEnemyClass(), Settings->GetPoolPrewarmCount(), Settings->GetPoolMaxSize());
	}
}

UWorld* URSRoundDirector::GetWorld() const
{
	return GameMode.IsValid() ? GameMode->GetWorld() : nullptr;
}

ARSGameState* URSRoundDirector::GetRSGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ARSGameState>() : nullptr;
}

int32 URSRoundDirector::GetPlayerCount() const
{
	const ARSGameState* GameState = GetRSGameState();
	return GameState ? FMath::Max(GameState->PlayerArray.Num(), 1) : 1;
}

void URSRoundDirector::SetPhase(ERSRoundPhase NewPhase)
{
	Phase = NewPhase;

	if (ARSGameState* GameState = GetRSGameState())
	{
		GameState->SetRoundPhase(NewPhase);
	}
}

void URSRoundDirector::PublishCounts()
{
	if (ARSGameState* GameState = GetRSGameState())
	{
		GameState->SetZombieCounts(LiveEnemies.Num(), RemainingToSpawn);
	}
}

void URSRoundDirector::StartMatch()
{
	if (!Settings)
	{
		UE_LOG(LogRSRounds, Error, TEXT("Round director started with no settings asset; no round will run."));
		return;
	}

	RoundNumber = 0;
	EnterIntermission();
}

void URSRoundDirector::StopMatch()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

void URSRoundDirector::AbandonHorde()
{
	HordeRemaining = 0;

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HordeTimerHandle);
	}
}

void URSRoundDirector::EnterIntermission()
{
	StopMatch();

	// The number moves here, at the start of the breathing room, so the round the player is about
	// to fight is the one on the wall while they are spending scrap for it.
	++RoundNumber;
	RemainingToSpawn = 0;

	if (ARSGameState* GameState = GetRSGameState())
	{
		GameState->SetRoundNumber(RoundNumber);
	}

	SetPhase(ERSRoundPhase::Intermission);
	PublishCounts();

	// The dead come back as a round ends, not mid-fight, and in time to spend the intermission.
	if (GameMode.IsValid())
	{
		GameMode->RespawnDeadPlayers();
	}

	const float IntermissionSeconds = Settings->GetIntermissionSeconds(RoundNumber);

	UE_LOG(LogRSRounds, Log, TEXT("Round %d: intermission, %.1f s."), RoundNumber, IntermissionSeconds);

	if (IntermissionSeconds > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::EnterSpawning, IntermissionSeconds, /*bLoop=*/ false);
	}
	else
	{
		EnterSpawning();
	}
}

void URSRoundDirector::EnterSpawning()
{
	const int32 PlayerCount = GetPlayerCount();

	RemainingToSpawn = Settings->GetEnemyQuota(RoundNumber, PlayerCount);

	SetPhase(ERSRoundPhase::Spawning);
	PublishCounts();

	const float SpawnInterval = Settings->GetSpawnInterval(RoundNumber);

	UE_LOG(LogRSRounds, Log, TEXT("Round %d: releasing %d enemies, one every %.2f s, at most %d alive (%d players)."),
		RoundNumber, RemainingToSpawn, SpawnInterval, Settings->GetMaxAlive(PlayerCount), PlayerCount);

	// First one now rather than one interval from now: the intermission was the pause, and the
	// round should start the moment it ends.
	GetWorld()->GetTimerManager().SetTimer(SpawnTimerHandle, this, &ThisClass::SpawnTick, SpawnInterval,
		/*bLoop=*/ true, /*InFirstDelay=*/ 0.0f);
}

void URSRoundDirector::EnterCleanup()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	SetPhase(ERSRoundPhase::Cleanup);
	PublishCounts();

	UE_LOG(LogRSRounds, Log, TEXT("Round %d: quota released, %d still alive."), RoundNumber, LiveEnemies.Num());

	// A round whose quota could not release a single enemy, or one the players finished off as
	// the last one spawned, ends here rather than waiting for a death that will never come.
	if (LiveEnemies.Num() == 0)
	{
		EnterIntermission();
	}
}

void URSRoundDirector::SpawnTick()
{
	if (RemainingToSpawn <= 0)
	{
		EnterCleanup();
		return;
	}

	// The ceiling is a pause, not a loss: the quota waits until the players have thinned the
	// horde. This is what keeps the enemy count inside the performance budget at every round.
	if (LiveEnemies.Num() >= Settings->GetMaxAlive(GetPlayerCount()))
	{
		return;
	}

	if (ReleaseOneEnemy())
	{
		--RemainingToSpawn;
		PublishCounts();

		if (RemainingToSpawn <= 0)
		{
			EnterCleanup();
		}
	}
}

ARSZombieCharacter* URSRoundDirector::ReleaseOneEnemy()
{
	URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this);
	if (!SpawnSubsystem)
	{
		return nullptr;
	}

	ARSZombieCharacter* Enemy = SpawnSubsystem->SpawnEnemy(Settings->GetEnemyClass(), Settings->GetSpawnSelector());
	if (!Enemy)
	{
		// No point was usable this tick — every one occupied, cooling down or behind a closed
		// zone. The timer comes round again; nothing is lost but the interval.
		return nullptr;
	}

	ApplyRoundScaling(*Enemy);

	// Watched for this life only: ForgetEnemy lets go of all three, because a pooled body comes
	// back as somebody else's enemy, or this director's again in a later round.
	Enemy->OnZombieDied.AddUObject(this, &ThisClass::HandleEnemyDied);
	Enemy->OnReturnedToPool.AddUObject(this, &ThisClass::HandleEnemyReturnedToPool);
	Enemy->OnDestroyed.AddDynamic(this, &ThisClass::HandleEnemyDestroyed);
	LiveEnemies.Add(Enemy);

	return Enemy;
}

void URSRoundDirector::ApplyRoundScaling(ARSZombieCharacter& Enemy) const
{
	Enemy.SetMoveTier(Settings->DrawMoveTier(RoundNumber));

	const TSubclassOf<UGameplayEffect> ScalingEffect = Settings->GetScalingEffect();
	URSAbilitySystemComponent* ASC = Enemy.GetRSAbilitySystemComponent();
	if (!ScalingEffect || !ASC)
	{
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(Settings);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ScalingEffect, 1.0f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// One magnitude for both attributes. The effect overrides MaxHealth and Health from it, so an
	// enemy arrives at full health for its round instead of at whatever its ability set gave it.
	SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Health, Settings->GetEnemyHealth(RoundNumber));

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void URSRoundDirector::ForgetEnemy(ARSZombieCharacter* Enemy)
{
	StopWatching(Enemy);

	LiveEnemies.RemoveAllSwap([Enemy](const TWeakObjectPtr<ARSZombieCharacter>& Live)
	{
		return !Live.IsValid() || Live.Get() == Enemy;
	});

	PublishCounts();
}

void URSRoundDirector::StopWatching(ARSZombieCharacter* Enemy)
{
	if (Enemy)
	{
		Enemy->OnZombieDied.RemoveAll(this);
		Enemy->OnReturnedToPool.RemoveAll(this);
		Enemy->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleEnemyDestroyed);
	}
}

void URSRoundDirector::CheckRoundComplete()
{
	// Only the cleanup phase is waiting for a body to leave. During spawning the quota carries the
	// round and a death just makes room under the ceiling.
	if (Phase == ERSRoundPhase::Cleanup && LiveEnemies.Num() == 0)
	{
		EnterIntermission();
	}
}

void URSRoundDirector::HandleEnemyDied(ARSZombieCharacter* Enemy, AActor* Killer, bool bHeadshot)
{
	// Before the round can end on it, so a kill that closes a round still counts in that round.
	OnEnemyKilled.Broadcast(Enemy, Killer, bHeadshot);

	ForgetEnemy(Enemy);
	CheckRoundComplete();
}

void URSRoundDirector::HandleEnemyReturnedToPool(ARSZombieCharacter* Enemy)
{
	// A death has already forgotten a corpse, and stopped watching it, so this only ever hears a
	// living enemy taken out of the world: the round still has to end without it.
	ForgetEnemy(Enemy);
	CheckRoundComplete();
}

void URSRoundDirector::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	// The same for a body destroyed rather than pooled: a travel, a full pool, pooling switched off.
	ForgetEnemy(Cast<ARSZombieCharacter>(DestroyedActor));
	CheckRoundComplete();
}

void URSRoundDirector::EnterGameOver()
{
	if (Phase == ERSRoundPhase::GameOver)
	{
		return;
	}

	StopMatch();
	AbandonHorde();

	// The enemies still alive stay where they are: with nobody standing they have no one to chase.
	SetPhase(ERSRoundPhase::GameOver);

	UE_LOG(LogRSRounds, Log, TEXT("Round %d: nobody standing, game over."), RoundNumber);
}

void URSRoundDirector::SkipToRound(int32 NewRoundNumber)
{
	// Starting a round would bring the dead back and carry on a match that has been lost.
	if (Phase == ERSRoundPhase::GameOver)
	{
		UE_LOG(LogRSRounds, Warning, TEXT("The match is over; restart it rather than skipping to a round."));
		return;
	}

	StopMatch();

	// A horde queued for the round being left behind has no business filling the next one.
	AbandonHorde();

	// Out of the phase first, and each enemy unwatched before it goes: a release that reached the
	// director while cleanup was still the phase, emptying the list, would start the next round
	// from inside this loop.
	SetPhase(ERSRoundPhase::WaitingToStart);

	// Clear the board through the same door the enemies came in by, rather than killing them:
	// a forced round should not pay scrap for sixty kills the player did not make.
	URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this);
	TArray<TWeakObjectPtr<ARSZombieCharacter>> Leaving = MoveTemp(LiveEnemies);
	LiveEnemies.Reset();

	for (const TWeakObjectPtr<ARSZombieCharacter>& Live : Leaving)
	{
		if (Live.IsValid() && SpawnSubsystem)
		{
			StopWatching(Live.Get());
			SpawnSubsystem->ReleaseEnemy(Live.Get());
		}
	}

	RoundNumber = FMath::Max(NewRoundNumber, 1) - 1;
	EnterIntermission();
}

int32 URSRoundDirector::SpawnHorde(int32 Count)
{
	if (Count <= 0 || !Settings)
	{
		return 0;
	}

	HordeRemaining += Count;

	const UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(HordeTimerHandle))
	{
		World->GetTimerManager().SetTimer(HordeTimerHandle, this, &ThisClass::HordeTick,
			RSRoundDirector::HordeReleaseInterval, /*bLoop=*/ true, /*InFirstDelay=*/ 0.0f);
	}

	return Count;
}

void URSRoundDirector::HordeTick()
{
	if (HordeRemaining <= 0)
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HordeTimerHandle);
		}
		return;
	}

	// One pass over every point each tick. A point that is cooling down or occupied simply is not
	// used this pass, and the queue waits rather than dropping what it could not place.
	const int32 Attempts = FMath::Min(HordeRemaining, FMath::Max(URSSpawnSubsystem::Get(this) ? URSSpawnSubsystem::Get(this)->GetSpawners().Num() : 1, 1));

	for (int32 Index = 0; Index < Attempts; ++Index)
	{
		if (!ReleaseOneEnemy())
		{
			break;
		}

		--HordeRemaining;
	}

	PublishCounts();
}
