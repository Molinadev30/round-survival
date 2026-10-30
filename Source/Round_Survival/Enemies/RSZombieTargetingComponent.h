// Round Survival

#pragma once

#include "AI/Navigation/NavigationTypes.h"
#include "Components/ControllerComponent.h"

#include "RSZombieTargetingComponent.generated.h"

class APawn;

DECLARE_MULTICAST_DELEGATE_OneParam(FRSZombieTargetChanged, APawn* /*NewTarget*/);

/**
 * Picks which player one enemy is going after.
 *
 * Deliberately not a StateTree evaluator. Target selection is the most expensive thing an enemy
 * does and it has to keep running whatever state the tree is in — tearing a barricade, vaulting
 * a window, recovering from a stagger. Owning the cost here means it can be staggered across
 * agents and budgeted on its own, and the round director can ask an enemy what it is chasing
 * without walking into its behaviour tree.
 *
 * **Path distance, never euclidean.** A player on the other side of a wall is metres away and
 * a corridor's length in travel; picking by straight-line distance produces the classic bug of
 * a horde pressing against a wall while an open door stands two rooms over. Each candidate costs
 * one asynchronous path query, which is why this runs on a slow staggered timer rather than on
 * tick, and why a single living player is resolved without querying at all.
 *
 * Server only: it lives on the AI controller, which never exists on a client.
 */
UCLASS(ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class ROUND_SURVIVAL_API URSZombieTargetingComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	URSZombieTargetingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Starts the refresh timer. Called from the controller on possession. */
	void StartTargeting();

	/** Stops the refresh timer and drops the current target. Called on unpossession. */
	void StopTargeting();

	/** The player this enemy is going after, or null while there is nobody reachable. */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	APawn* GetTarget() const { return CurrentTarget.Get(); }

	/** Travel distance to the current target as of the last refresh. Negative when unknown. */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	float GetTargetPathDistance() const { return CurrentPathDistance; }

	/** Fired on the server when the target changes, including when it becomes null. */
	FRSZombieTargetChanged OnTargetChanged;

protected:
	//~UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	/** Issues one round of path queries, or resolves the target outright when it is unambiguous. */
	void RefreshTarget();

	/** Collects the living player pawns worth querying a path to. */
	void GatherCandidates(TArray<APawn*>& OutCandidates) const;

	/** Async path query answer. Keeps the shortest and settles the refresh once all have come in. */
	void HandlePathFound(uint32 QueryID, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path);

	/** Applies the winner of a finished refresh and fires the delegate if it changed. */
	void SetTarget(APawn* NewTarget, float PathDistance);

	/** Seconds between refreshes. The first one is offset randomly so the horde does not sync up. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Enemy", meta = (ForceUnits = s, ClampMin = 0.05))
	float RefreshInterval = 0.5f;

	/**
	 * Travel distance past which a player is not worth chasing. Zero means no limit.
	 *
	 * Left at zero by default: the map is small and an enemy that gives up on everyone would
	 * just stand still.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Enemy", meta = (ForceUnits = cm, ClampMin = 0.0))
	float MaxTargetPathDistance = 0.0f;

private:
	/** Who we are chasing right now. Weak: the target can die or leave between refreshes. */
	TWeakObjectPtr<APawn> CurrentTarget;

	float CurrentPathDistance = -1.0f;

	/**
	 * Path queries still in flight for the refresh in progress, by query id.
	 *
	 * Starting a refresh empties this, so an answer that arrives from an abandoned round finds
	 * no entry and is dropped. That is the whole staleness guard: no serial numbers, and a
	 * query that never comes back leaks nothing but its own entry until the next refresh.
	 */
	TMap<uint32, TWeakObjectPtr<APawn>> PendingQueries;

	/** Best candidate of the refresh in progress. */
	TWeakObjectPtr<APawn> BestCandidate;

	float BestPathDistance = TNumericLimits<float>::Max();

	FTimerHandle RefreshTimerHandle;
};
