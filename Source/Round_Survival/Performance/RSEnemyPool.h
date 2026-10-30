// Round Survival

#pragma once

#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "RSEnemyPool.generated.h"

class ARSZombieCharacter;

/** The bodies of one enemy class waiting to be handed out again. */
USTRUCT()
struct FRSEnemyPoolBucket
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<ARSZombieCharacter>> Free;
};

/**
 * Enemies that have left the world, kept to be handed out again instead of destroyed.
 *
 * A round releases enemies a few a second and takes them back as fast as the players kill them, so
 * a match spawns and destroys hundreds of characters, each with a skeletal mesh, an ability system,
 * two attribute sets, a movement component and an AI controller. Spawning one is the most expensive
 * thing the round director does, and it does it mid-fight. Here a body is a pop off a list.
 *
 * This class only keeps the lists. Taking a body out of the world and putting it back are the
 * enemy's own business (`ARSZombieCharacter::ReturnToPool` and `LeavePool`), and deciding when to
 * pool is the spawn subsystem's, the one door enemies come through. Server only.
 */
UCLASS()
class ROUND_SURVIVAL_API URSEnemyPool : public UObject
{
	GENERATED_BODY()

public:
	/** A pooled body of this class, off its list, or null when there is none. It is still in the pool's state: the caller brings it back. */
	ARSZombieCharacter* Take(TSubclassOf<ARSZombieCharacter> EnemyClass);

	/**
	 * Puts a body that is already out of the world on its class's list. False when the list is at
	 * MaxPerClass, and then the caller destroys it: a pool that only ever grows would keep the
	 * peak of the worst round alive for the rest of the match.
	 */
	bool Give(ARSZombieCharacter* Enemy);

	/** Whether Give would take one more body of this class. */
	bool HasRoomFor(TSubclassOf<ARSZombieCharacter> EnemyClass) const;

	int32 GetNumFree(TSubclassOf<ARSZombieCharacter> EnemyClass) const;

	/** Bodies waiting, every class together. */
	int32 GetNumFreeTotal() const;

	void SetMaxPerClass(int32 InMaxPerClass) { MaxPerClass = FMath::Max(InMaxPerClass, 0); }
	int32 GetMaxPerClass() const { return MaxPerClass; }

	/** Bodies handed out from a list, and the requests that found the list empty and had to spawn. */
	int32 GetNumHits() const { return NumHits; }
	int32 GetNumMisses() const { return NumMisses; }

	/** Records a request a spawn had to answer. The pool cannot see those itself. */
	void NoteMiss() { ++NumMisses; }

	void ResetCounts() { NumHits = 0; NumMisses = 0; }

private:
	UPROPERTY()
	TMap<TSubclassOf<ARSZombieCharacter>, FRSEnemyPoolBucket> Buckets;

	int32 MaxPerClass = 80;
	int32 NumHits = 0;
	int32 NumMisses = 0;
};
