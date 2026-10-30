// Round Survival

#include "Performance/RSEnemyPool.h"

#include "Enemies/RSZombieCharacter.h"

ARSZombieCharacter* URSEnemyPool::Take(TSubclassOf<ARSZombieCharacter> EnemyClass)
{
	FRSEnemyPoolBucket* Bucket = Buckets.Find(EnemyClass);
	while (Bucket && Bucket->Free.Num() > 0)
	{
		// Last in, first out: the body most recently put away is the one most likely still warm in
		// the caches. Something outside the pool may have destroyed one meanwhile — a travel, a
		// debug tool — so a dead entry is skipped, not handed out.
		ARSZombieCharacter* Enemy = Bucket->Free.Pop(EAllowShrinking::No);
		if (IsValid(Enemy))
		{
			++NumHits;
			return Enemy;
		}
	}

	return nullptr;
}

bool URSEnemyPool::Give(ARSZombieCharacter* Enemy)
{
	if (!IsValid(Enemy) || !HasRoomFor(Enemy->GetClass()))
	{
		return false;
	}

	Buckets.FindOrAdd(Enemy->GetClass()).Free.Add(Enemy);
	return true;
}

bool URSEnemyPool::HasRoomFor(TSubclassOf<ARSZombieCharacter> EnemyClass) const
{
	return GetNumFree(EnemyClass) < MaxPerClass;
}

int32 URSEnemyPool::GetNumFree(TSubclassOf<ARSZombieCharacter> EnemyClass) const
{
	const FRSEnemyPoolBucket* Bucket = Buckets.Find(EnemyClass);
	return Bucket ? Bucket->Free.Num() : 0;
}

int32 URSEnemyPool::GetNumFreeTotal() const
{
	int32 Total = 0;
	for (const TPair<TSubclassOf<ARSZombieCharacter>, FRSEnemyPoolBucket>& Pair : Buckets)
	{
		Total += Pair.Value.Free.Num();
	}
	return Total;
}
