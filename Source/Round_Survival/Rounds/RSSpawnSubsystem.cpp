// Round Survival

#include "Rounds/RSSpawnSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Performance/RSEnemyPool.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "Rounds/RSSpawnSelector.h"
#include "Rounds/RSZombieSpawner.h"
#include "Rounds/RSZone.h"

namespace RSConsoleVariables
{
	static bool bEnemyPoolingEnabled = true;
	static FAutoConsoleVariableRef CVarEnemyPoolingEnabled(
		TEXT("RS.Pool.Enabled"),
		bEnemyPoolingEnabled,
		TEXT("Hands out pooled enemies and pools the ones that leave. 0 spawns and destroys every one, to compare."),
		ECVF_Default);
}

DECLARE_STATS_GROUP(TEXT("RS Enemy Pool"), STATGROUP_RSEnemyPool, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Enemy reused from the pool"), STAT_RSEnemyReuse, STATGROUP_RSEnemyPool);
DECLARE_CYCLE_STAT(TEXT("Enemy spawned"), STAT_RSEnemySpawn, STATGROUP_RSEnemyPool);
DECLARE_CYCLE_STAT(TEXT("Enemy returned to the pool"), STAT_RSEnemyPooled, STATGROUP_RSEnemyPool);
DECLARE_CYCLE_STAT(TEXT("Enemy destroyed"), STAT_RSEnemyDestroy, STATGROUP_RSEnemyPool);

namespace RSSpawn
{
	/** Adds a scope's wall time to a pool timing, for RS.PoolStats. The cycle stat beside each is for Insights. */
	struct FScopedPoolTiming
	{
		explicit FScopedPoolTiming(FRSEnemyPoolTiming& InTiming)
			: Timing(InTiming)
			, StartSeconds(FPlatformTime::Seconds())
		{
		}

		~FScopedPoolTiming()
		{
			Timing.Add(FPlatformTime::Seconds() - StartSeconds);
		}

		FRSEnemyPoolTiming& Timing;
		double StartSeconds;
	};

	/**
	 * Where prewarmed bodies are made, well under any level. They are out of the world before the
	 * spawn returns, but a body made in a room would be made inside whoever stands there.
	 */
	static const FVector PrewarmLocation(0.0, 0.0, -20000.0);
}

URSSpawnSubsystem* URSSpawnSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URSSpawnSubsystem>() : nullptr;
}

bool URSSpawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Not in the editor world: a spawner placed in the level registering while nobody is playing
	// would hand the round director points from a world that is not running.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URSSpawnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Pool = NewObject<URSEnemyPool>(this);
}

void URSSpawnSubsystem::Deinitialize()
{
	Zones.Reset();
	Spawners.Reset();
	DefaultSelector = nullptr;
	Pool = nullptr;

	Super::Deinitialize();
}

bool URSSpawnSubsystem::IsPoolingEnabled()
{
	return RSConsoleVariables::bEnemyPoolingEnabled;
}

void URSSpawnSubsystem::RegisterZone(ARSZone* Zone)
{
	if (IsValid(Zone))
	{
		Zones.AddUnique(Zone);
	}
}

void URSSpawnSubsystem::UnregisterZone(ARSZone* Zone)
{
	Zones.Remove(Zone);
}

void URSSpawnSubsystem::RegisterSpawner(ARSZombieSpawner* Spawner)
{
	if (IsValid(Spawner))
	{
		Spawners.AddUnique(Spawner);
	}
}

void URSSpawnSubsystem::UnregisterSpawner(ARSZombieSpawner* Spawner)
{
	Spawners.Remove(Spawner);
}

void URSSpawnSubsystem::GatherEligibleSpawners(TArray<ARSZombieSpawner*>& OutSpawners) const
{
	OutSpawners.Reset();
	OutSpawners.Reserve(Spawners.Num());

	for (const TObjectPtr<ARSZombieSpawner>& Spawner : Spawners)
	{
		if (IsValid(Spawner) && Spawner->IsEligible())
		{
			OutSpawners.Add(Spawner);
		}
	}
}

void URSSpawnSubsystem::GatherStandingPlayers(TArray<APawn*>& OutPlayers) const
{
	OutPlayers.Reset();

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			continue;
		}

		// Asked of the ability system, by the same tags the enemy targeting uses, so spawn placement
		// weighs points against the players the horde is actually going for.
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (ASC && (ASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead) || ASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed)))
		{
			continue;
		}

		OutPlayers.Add(Pawn);
	}
}

const URSSpawnSelector* URSSpawnSubsystem::GetDefaultSelector()
{
	if (!DefaultSelector)
	{
		DefaultSelector = NewObject<URSSpawnSelector_Weighted>(this);
	}

	return DefaultSelector;
}

ARSZombieCharacter* URSSpawnSubsystem::SpawnEnemy(TSubclassOf<ARSZombieCharacter> EnemyClass, const URSSpawnSelector* Selector)
{
	TArray<ARSZombieSpawner*> Candidates;
	GatherEligibleSpawners(Candidates);

	if (Candidates.Num() == 0)
	{
		// Every point is closed, cooling down or occupied. The caller retries next interval; this
		// is a normal state at the start of a round, not an error.
		UE_LOG(LogRSRounds, Verbose, TEXT("No eligible spawn point among %d registered."), Spawners.Num());
		return nullptr;
	}

	TArray<APawn*> Players;
	GatherStandingPlayers(Players);

	const URSSpawnSelector* EffectiveSelector = Selector ? Selector : GetDefaultSelector();
	ARSZombieSpawner* ChosenSpawner = EffectiveSelector ? EffectiveSelector->SelectSpawner(Candidates, Players) : Candidates[0];

	return SpawnEnemyAt(EnemyClass, ChosenSpawner);
}

ARSZombieCharacter* URSSpawnSubsystem::SpawnEnemyAt(TSubclassOf<ARSZombieCharacter> EnemyClass, ARSZombieSpawner* Spawner)
{
	if (!IsValid(Spawner))
	{
		return nullptr;
	}

	ARSZombieCharacter* Enemy = AcquireEnemy(EnemyClass, Spawner->GetGroundSpawnTransform());
	if (Enemy)
	{
		Spawner->NotifyEnemySpawned(Enemy);

		UE_LOG(LogRSRounds, Verbose, TEXT("Released [%s] from [%s]."), *GetNameSafe(Enemy), *GetNameSafe(Spawner));
	}

	return Enemy;
}

ARSZombieCharacter* URSSpawnSubsystem::AcquireEnemy(TSubclassOf<ARSZombieCharacter> EnemyClass, const FTransform& GroundTransform)
{
	UWorld* World = GetWorld();
	if (!World || !EnemyClass)
	{
		return nullptr;
	}

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRSRounds, Error, TEXT("Enemy spawn requested on a client. Enemies exist because the server said so."));
		return nullptr;
	}

	// The spawn point is on the floor and an actor's origin is the middle of its capsule, so the
	// class defaults decide the lift. Reading it from the CDO keeps a taller enemy from needing
	// its own spawn points.
	FTransform SpawnTransform = GroundTransform;
	if (const ARSZombieCharacter* EnemyDefaults = EnemyClass->GetDefaultObject<ARSZombieCharacter>())
	{
		if (const UCapsuleComponent* Capsule = EnemyDefaults->GetCapsuleComponent())
		{
			SpawnTransform.AddToTranslation(FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight()));
		}
	}

	if (IsPoolingEnabled() && Pool)
	{
		if (ARSZombieCharacter* Pooled = Pool->Take(EnemyClass))
		{
			SCOPE_CYCLE_COUNTER(STAT_RSEnemyReuse);
			RSSpawn::FScopedPoolTiming Timing(ReuseTiming);

			Pooled->LeavePool(SpawnTransform);
			return Pooled;
		}

		Pool->NoteMiss();
	}

	SCOPE_CYCLE_COUNTER(STAT_RSEnemySpawn);
	RSSpawn::FScopedPoolTiming Timing(SpawnTiming);

	return SpawnNewEnemy(EnemyClass, SpawnTransform);
}

ARSZombieCharacter* URSSpawnSubsystem::SpawnNewEnemy(TSubclassOf<ARSZombieCharacter> EnemyClass, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;

	return GetWorld()->SpawnActor<ARSZombieCharacter>(EnemyClass, SpawnTransform, SpawnParameters);
}

void URSSpawnSubsystem::ReleaseEnemy(ARSZombieCharacter* Enemy)
{
	if (!IsValid(Enemy) || Enemy->IsInPool())
	{
		return;
	}

	if (IsPoolingEnabled() && Pool && Pool->HasRoomFor(Enemy->GetClass()))
	{
		SCOPE_CYCLE_COUNTER(STAT_RSEnemyPooled);
		RSSpawn::FScopedPoolTiming Timing(PoolTiming);

		Enemy->ReturnToPool();
		Pool->Give(Enemy);
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_RSEnemyDestroy);
	RSSpawn::FScopedPoolTiming Timing(DestroyTiming);

	Enemy->Destroy();
}

void URSSpawnSubsystem::LogPoolStats() const
{
	UE_LOG(LogRSRounds, Display, TEXT("Enemy pool %s: %d waiting, at most %d of a class; %d handed out from the pool, %d spawned on empty."),
		IsPoolingEnabled() ? TEXT("on") : TEXT("off"), Pool ? Pool->GetNumFreeTotal() : 0, Pool ? Pool->GetMaxPerClass() : 0,
		Pool ? Pool->GetNumHits() : 0, Pool ? Pool->GetNumMisses() : 0);

	const auto LogTiming = [](const TCHAR* Label, const FRSEnemyPoolTiming& Timing)
	{
		UE_LOG(LogRSRounds, Display, TEXT("  %-9s %4d x %7.1f us average, %7.1f us at most"),
			Label, Timing.Count, Timing.GetAverageMicroseconds(), Timing.MaxSeconds * 1.0e6);
	};

	LogTiming(TEXT("reused"), ReuseTiming);
	LogTiming(TEXT("spawned"), SpawnTiming);
	LogTiming(TEXT("pooled"), PoolTiming);
	LogTiming(TEXT("destroyed"), DestroyTiming);
}

void URSSpawnSubsystem::ResetPoolStats()
{
	ReuseTiming = FRSEnemyPoolTiming();
	SpawnTiming = FRSEnemyPoolTiming();
	PoolTiming = FRSEnemyPoolTiming();
	DestroyTiming = FRSEnemyPoolTiming();

	if (Pool)
	{
		Pool->ResetCounts();
	}
}

void URSSpawnSubsystem::PrewarmPool(TSubclassOf<ARSZombieCharacter> EnemyClass, int32 Count, int32 MaxPerClass)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !EnemyClass || !Pool)
	{
		return;
	}

	Pool->SetMaxPerClass(MaxPerClass);
	if (!IsPoolingEnabled())
	{
		return;
	}

	const int32 ToMake = FMath::Min(Count, MaxPerClass) - Pool->GetNumFree(EnemyClass);
	const double StartSeconds = FPlatformTime::Seconds();

	// Spawned whole — possessed, kit granted, running — and put straight away, the same way a body
	// that has finished dying is. A prewarmed enemy is not a special kind of pooled enemy.
	for (int32 Index = 0; Index < ToMake; ++Index)
	{
		ARSZombieCharacter* Enemy = SpawnNewEnemy(EnemyClass, FTransform(RSSpawn::PrewarmLocation));
		if (!Enemy)
		{
			break;
		}

		Enemy->ReturnToPool();
		Pool->Give(Enemy);
	}

	UE_LOG(LogRSRounds, Log, TEXT("Enemy pool: %d [%s] made in %.1f ms; %d waiting, at most %d."), FMath::Max(ToMake, 0), *GetNameSafe(EnemyClass),
		(FPlatformTime::Seconds() - StartSeconds) * 1000.0, Pool->GetNumFree(EnemyClass), MaxPerClass);
}
