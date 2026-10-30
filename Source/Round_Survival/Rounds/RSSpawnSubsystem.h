// Round Survival

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"

#include "RSSpawnSubsystem.generated.h"

class APawn;
class ARSZombieCharacter;
class ARSZombieSpawner;
class ARSZone;
class URSEnemyPool;
class URSSpawnSelector;

/** Wall time spent on one of the ways a body enters or leaves the world, for RS.PoolStats. */
struct FRSEnemyPoolTiming
{
	double TotalSeconds = 0.0;
	double MaxSeconds = 0.0;
	int32 Count = 0;

	void Add(double Seconds)
	{
		TotalSeconds += Seconds;
		MaxSeconds = FMath::Max(MaxSeconds, Seconds);
		++Count;
	}

	double GetAverageMicroseconds() const { return Count > 0 ? TotalSeconds / Count * 1.0e6 : 0.0; }
};

/**
 * The registry of zones and spawn points, and the one door enemies come through.
 *
 * Spawners and zones announce themselves here on BeginPlay instead of being found with an actor
 * iterator: the round director asks for a spawn point several times a second at the top of a
 * round, and walking every actor in the level to answer that is a cost that grows with the map
 * for no reason.
 *
 * It is also where enemies are pooled. Everything that puts an enemy into the world calls
 * `AcquireEnemy`, and everything that takes one out calls `ReleaseEnemy`: a pop off the pool
 * (URSEnemyPool) and a push back onto it, with a spawn when the pool is empty and a destroy when
 * it is full. Nothing else in the project calls `SpawnActor` for an enemy, which is what made
 * pooling a change behind these two calls. `RS.Pool.Enabled 0` goes back to spawning and
 * destroying, to compare the two.
 *
 * Server only in practice: it exists on clients but nothing ever asks it for anything.
 */
UCLASS()
class ROUND_SURVIVAL_API URSSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static URSSpawnSubsystem* Get(const UObject* WorldContextObject);

	void RegisterZone(ARSZone* Zone);
	void UnregisterZone(ARSZone* Zone);

	void RegisterSpawner(ARSZombieSpawner* Spawner);
	void UnregisterSpawner(ARSZombieSpawner* Spawner);

	const TArray<TObjectPtr<ARSZone>>& GetZones() const { return Zones; }
	const TArray<TObjectPtr<ARSZombieSpawner>>& GetSpawners() const { return Spawners; }

	/** Every registered point whose zone is open, cooldown has elapsed and floor is clear. */
	void GatherEligibleSpawners(TArray<ARSZombieSpawner*>& OutSpawners) const;

	/** Player pawns still standing — neither `RS.State.Downed` nor `RS.State.Dead` — by the same test the enemy targeting uses. */
	void GatherStandingPlayers(TArray<APawn*>& OutPlayers) const;

	/**
	 * Puts one enemy into the world at a point the selector chooses.
	 *
	 * @param Selector  Policy to pick with. Null uses a default weighted selector.
	 * @return The enemy, or null when nothing could be released this call.
	 */
	ARSZombieCharacter* SpawnEnemy(TSubclassOf<ARSZombieCharacter> EnemyClass, const URSSpawnSelector* Selector = nullptr);

	/** Puts one enemy into the world at a named point, whatever the selector would have said. */
	ARSZombieCharacter* SpawnEnemyAt(TSubclassOf<ARSZombieCharacter> EnemyClass, ARSZombieSpawner* Spawner);

	/**
	 * Produces a live enemy at a transform: one from the pool when it has one, a new one otherwise.
	 *
	 * @param GroundTransform  On the floor. The capsule is lifted here, from the class defaults.
	 */
	ARSZombieCharacter* AcquireEnemy(TSubclassOf<ARSZombieCharacter> EnemyClass, const FTransform& GroundTransform);

	/** Takes an enemy out of the world, dead or alive: onto the pool when it has room, destroyed otherwise. */
	void ReleaseEnemy(ARSZombieCharacter* Enemy);

	/**
	 * Sizes the pool and fills it with Count bodies of EnemyClass, so the spawns a round would pay
	 * for mid-fight are paid while the match is starting. Server only.
	 */
	void PrewarmPool(TSubclassOf<ARSZombieCharacter> EnemyClass, int32 Count, int32 MaxPerClass);

	const URSEnemyPool* GetPool() const { return Pool; }

	/** RS.Pool.Enabled. */
	static bool IsPoolingEnabled();

	/**
	 * Logs what the pool holds and what each way in and out has cost since the last reset: a body
	 * reused against one spawned, and a body pooled against one destroyed. Wall time on this
	 * machine, so compare the two halves of one run rather than numbers across builds. RS.PoolStats.
	 */
	void LogPoolStats() const;

	void ResetPoolStats();

protected:
	//~USubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End of USubsystem interface

	/** A new enemy, possessed and running, at a transform whose capsule is already lifted off the floor. */
	ARSZombieCharacter* SpawnNewEnemy(TSubclassOf<ARSZombieCharacter> EnemyClass, const FTransform& SpawnTransform);

	/** Built on demand so a caller with no configured policy still gets sensible placement. */
	const URSSpawnSelector* GetDefaultSelector();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ARSZone>> Zones;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ARSZombieSpawner>> Spawners;

	UPROPERTY(Transient)
	TObjectPtr<URSSpawnSelector> DefaultSelector;

	UPROPERTY(Transient)
	TObjectPtr<URSEnemyPool> Pool;

	/**
	 * Each from the call to the enemy standing possessed with its kit (in) or out of the world (out).
	 * The round's scaling is the caller's and is in neither. A destroy's garbage collection comes
	 * later and is not in it either.
	 */
	FRSEnemyPoolTiming ReuseTiming;
	FRSEnemyPoolTiming SpawnTiming;
	FRSEnemyPoolTiming PoolTiming;
	FRSEnemyPoolTiming DestroyTiming;
};
