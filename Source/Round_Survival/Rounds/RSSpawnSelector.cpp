// Round Survival

#include "Rounds/RSSpawnSelector.h"

#include "GameFramework/Pawn.h"
#include "Rounds/RSZombieSpawner.h"

namespace RSSpawnSelector
{
	/**
	 * Distance floor for the weight, in centimetres. Not a gameplay rule: it only keeps a point
	 * standing on a player from dividing by zero and taking the whole draw.
	 */
	static constexpr float MinWeightDistance = 100.0f;
}

UWorld* URSSpawnSelector::GetWorld() const
{
	// Instanced on a data asset, so the world has to come from the outer chain. The CDO has none,
	// and returning a world for it would make the editor treat this as a spawnable object.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UObject* Outer = GetOuter();
	return Outer ? Outer->GetWorld() : nullptr;
}

ARSZombieSpawner* URSSpawnSelector_Weighted::SelectSpawner(const TArray<ARSZombieSpawner*>& Candidates, const TArray<APawn*>& Players) const
{
	float TotalWeight = 0.0f;
	TArray<float, TInlineAllocator<16>> Weights;
	Weights.Reserve(Candidates.Num());

	for (const ARSZombieSpawner* Candidate : Candidates)
	{
		const float Weight = Candidate ? FMath::Max(ScoreSpawner(*Candidate, Players), 0.0f) : 0.0f;

		Weights.Add(Weight);
		TotalWeight += Weight;
	}

	// Only reachable when every candidate was null. Nothing scores zero on its own merits.
	if (TotalWeight <= 0.0f)
	{
		return Candidates.Num() > 0 ? Candidates[0] : nullptr;
	}

	// Weighted draw rather than the best score: a wave that all came through the nearest door
	// would arrive as a queue, and the same door would win again next time.
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.0f)
		{
			return Candidates[Index];
		}
	}

	return Candidates.Last();
}

float URSSpawnSelector_Weighted::ScoreSpawner(const ARSZombieSpawner& Spawner, const TArray<APawn*>& Players) const
{
	// Nobody alive to measure against. Everything is equally good and the round is over anyway.
	if (Players.Num() == 0)
	{
		return 1.0f;
	}

	const FVector SpawnLocation = Spawner.GetActorLocation();

	float NearestDistanceSq = TNumericLimits<float>::Max();
	for (const APawn* Player : Players)
	{
		if (IsValid(Player))
		{
			NearestDistanceSq = FMath::Min(NearestDistanceSq, static_cast<float>(FVector::DistSquared(SpawnLocation, Player->GetActorLocation())));
		}
	}

	if (NearestDistanceSq == TNumericLimits<float>::Max())
	{
		return 1.0f;
	}

	// Inverse distance: the near doors carry the round, the far ones stay in the draw.
	const float NearestDistance = FMath::Max(FMath::Sqrt(NearestDistanceSq), RSSpawnSelector::MinWeightDistance);
	return 1.0f / NearestDistance;
}
