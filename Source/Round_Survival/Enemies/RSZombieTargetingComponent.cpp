// Round Survival

#include "Enemies/RSZombieTargetingComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "VisualLogger/VisualLogger.h"

URSZombieTargetingComponent::URSZombieTargetingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Everything here runs off a timer. A component that ticks 64 times a frame to do nothing is
	// exactly the cost this class exists to avoid.
	PrimaryComponentTick.bCanEverTick = false;
}

void URSZombieTargetingComponent::StartTargeting()
{
	AController* OwningController = GetController<AController>();
	if (!OwningController || !OwningController->GetPawn())
	{
		return;
	}

	// The first refresh lands at a random point inside the interval. Sixty-four enemies spawned
	// on the same frame would otherwise run sixty-four path queries on the same frame, for ever.
	const float FirstDelay = FMath::FRandRange(KINDA_SMALL_NUMBER, RefreshInterval);
	GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ThisClass::RefreshTarget, RefreshInterval, /*bLoop=*/ true, FirstDelay);
}

void URSZombieTargetingComponent::StopTargeting()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	PendingQueries.Reset();
	BestCandidate.Reset();
	BestPathDistance = TNumericLimits<float>::Max();

	SetTarget(nullptr, -1.0f);
}

void URSZombieTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopTargeting();

	Super::EndPlay(EndPlayReason);
}

void URSZombieTargetingComponent::GatherCandidates(TArray<APawn*>& OutCandidates) const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Candidate = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!IsValid(Candidate))
		{
			continue;
		}

		// A downed or dead player is not a target: the horde leaves a downed player for whoever is
		// still standing, which is what gives a teammate the chance to revive them. Asked of the
		// ability system rather than of a bool on the pawn, so anything else that should take a
		// player off the board only has to set a tag.
		const UAbilitySystemComponent* CandidateASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate);
		if (CandidateASC && (CandidateASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead)
			|| CandidateASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed)))
		{
			continue;
		}

		OutCandidates.Add(Candidate);
	}
}

void URSZombieTargetingComponent::RefreshTarget()
{
	AController* OwningController = GetController<AController>();
	APawn* ControlledPawn = OwningController ? OwningController->GetPawn() : nullptr;
	if (!ControlledPawn)
	{
		return;
	}

	// Abandon whatever the previous round had in flight: its answers no longer have an entry here
	// and will be dropped on arrival.
	PendingQueries.Reset();
	BestCandidate.Reset();
	BestPathDistance = TNumericLimits<float>::Max();

	TArray<APawn*> Candidates;
	GatherCandidates(Candidates);

	if (Candidates.Num() == 0)
	{
		SetTarget(nullptr, -1.0f);
		return;
	}

	// One living player is the answer whatever the path costs: there is nothing to compare it
	// against. This is the common case in single player and it skips the query entirely.
	if (Candidates.Num() == 1)
	{
		SetTarget(Candidates[0], -1.0f);
		return;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const FNavAgentProperties& AgentProperties = ControlledPawn->GetNavAgentPropertiesRef();
	const FVector StartLocation = ControlledPawn->GetNavAgentLocation();

	const ANavigationData* NavData = NavSystem ? NavSystem->GetNavDataForProps(AgentProperties, StartLocation) : nullptr;
	if (!NavData)
	{
		// No navigation data built for this agent. Fall back to the nearest by straight line so the
		// enemy still does something, and say so once per refresh rather than silently misbehaving.
		UE_LOG(LogRSRounds, Warning, TEXT("[%s] no navigation data for the enemy agent; falling back to euclidean targeting."),
			*GetNameSafe(ControlledPawn));

		APawn* NearestCandidate = nullptr;
		float NearestDistanceSq = TNumericLimits<float>::Max();
		for (APawn* Candidate : Candidates)
		{
			const float DistanceSq = FVector::DistSquared(StartLocation, Candidate->GetActorLocation());
			if (DistanceSq < NearestDistanceSq)
			{
				NearestDistanceSq = DistanceSq;
				NearestCandidate = Candidate;
			}
		}

		SetTarget(NearestCandidate, -1.0f);
		return;
	}

	const FSharedConstNavQueryFilter QueryFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, ControlledPawn, nullptr);

	for (APawn* Candidate : Candidates)
	{
		FPathFindingQuery Query(ControlledPawn, *NavData, StartLocation, Candidate->GetNavAgentLocation(), QueryFilter);

		// Asynchronous on purpose: the query is answered on a worker thread and delivered on the
		// game thread when it is done. Sixty-four enemies resolving paths synchronously inside a
		// timer would spike the frame they all landed on.
		const uint32 QueryID = NavSystem->FindPathAsync(AgentProperties, Query,
			FNavPathQueryDelegate::CreateUObject(this, &ThisClass::HandlePathFound), EPathFindingMode::Regular);

		if (QueryID != INVALID_NAVQUERYID)
		{
			PendingQueries.Add(QueryID, Candidate);
		}
	}

	if (PendingQueries.Num() == 0)
	{
		SetTarget(nullptr, -1.0f);
	}
}

void URSZombieTargetingComponent::HandlePathFound(uint32 QueryID, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path)
{
	TWeakObjectPtr<APawn> Candidate;
	if (!PendingQueries.RemoveAndCopyValue(QueryID, Candidate))
	{
		// An answer from a refresh that has already been abandoned.
		return;
	}

	if (Result == ENavigationQueryResult::Success && Path.IsValid() && Candidate.IsValid())
	{
		const float PathLength = static_cast<float>(Path->GetLength());
		const bool bWithinLimit = MaxTargetPathDistance <= 0.0f || PathLength <= MaxTargetPathDistance;

		if (bWithinLimit && PathLength < BestPathDistance)
		{
			BestPathDistance = PathLength;
			BestCandidate = Candidate;
		}
	}

	// The refresh settles on the last answer, not on a countdown: every query either lands here or
	// was dropped above, so an empty map means this round is done.
	if (PendingQueries.Num() == 0)
	{
		SetTarget(BestCandidate.Get(), BestCandidate.IsValid() ? BestPathDistance : -1.0f);
	}
}

void URSZombieTargetingComponent::SetTarget(APawn* NewTarget, float PathDistance)
{
	CurrentPathDistance = PathDistance;

	if (CurrentTarget.Get() == NewTarget)
	{
		return;
	}

	CurrentTarget = NewTarget;

	UE_VLOG(GetOwner(), LogRSRounds, Verbose, TEXT("Enemy target is now %s (%.0f cm of path)"), *GetNameSafe(NewTarget), PathDistance);

	OnTargetChanged.Broadcast(NewTarget);
}
