// Round Survival

#pragma once

#include "GameFramework/Actor.h"

#include "RSZombieSpawner.generated.h"

class ARSZombieCharacter;
class ARSBarricade;
class ARSZone;
class UArrowComponent;
class UBillboardComponent;

/**
 * One place enemies come into the world.
 *
 * A spawner is a point and a set of conditions, nothing more: it does not own the enemies it
 * released, does not count them and does not decide when to fire. The round director decides how
 * many, the spawn selector decides where, and this only answers whether it can be used right now
 * and hands back the transform to use.
 *
 * Server only — it never replicates. A client has no use for a spawn point, and sixty-four of
 * them in a level is sixty-four actors of channel traffic for nothing.
 *
 * A spawner can be bound to a barricade, and that binding is the whole of how an enemy knows
 * its way in: the window hands the enemy it releases a reference to itself, so nothing ever
 * searches the level for a way through. It deliberately does **not** gate eligibility on the
 * board count — a window that stopped feeding once its boards came off would pay the player for
 * breaking it and never repairing it, which is the opposite of what the repair economy is for.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSZombieSpawner : public AActor
{
	GENERATED_BODY()

public:
	ARSZombieSpawner(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The zone this point belongs to, or null when it is always available. */
	UFUNCTION(BlueprintPure, Category = "RS|Spawning")
	ARSZone* GetZone() const { return Zone; }

	/** The window enemies from this point come in through, or null when they simply walk in. */
	UFUNCTION(BlueprintPure, Category = "RS|Spawning")
	ARSBarricade* GetBarricade() const { return Barricade; }

	/** Whether the zone this point belongs to is open. A spawner with no zone always is. */
	UFUNCTION(BlueprintPure, Category = "RS|Spawning")
	bool IsZoneOpen() const;

	/** Whether the point is still inside the interval that follows its last release. */
	UFUNCTION(BlueprintPure, Category = "RS|Spawning")
	bool IsOnCooldown() const;

	/** Whether a pawn is standing on the point. Catches the body that has not walked off yet. */
	UFUNCTION(BlueprintPure, Category = "RS|Spawning")
	bool IsBlocked() const;

	/** Zone open, cooldown elapsed, nothing standing on it. What the selector filters on. */
	UFUNCTION(BlueprintPure, Category = "RS|Spawning")
	bool IsEligible() const;

	/**
	 * Where to put an enemy, on the ground.
	 *
	 * Projected onto the navigation mesh when `bProjectToNavigation` is set, so a point left
	 * floating or sunk by a level edit still produces an enemy that can walk. The caller lifts it
	 * by the capsule: this class does not know what is being spawned.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Spawning")
	FTransform GetGroundSpawnTransform() const;

	/** Starts the cooldown. Called by the spawn subsystem once a release has succeeded. */
	void NotifyEnemySpawned(ARSZombieCharacter* Enemy);

	/** Seconds since this point last released an enemy. Large when it never has. */
	float GetTimeSinceLastSpawn() const;

protected:
	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	/** The zone that gates this point. Left empty, the point is always available. */
	UPROPERTY(EditInstanceOnly, Category = "RS|Spawning")
	TObjectPtr<ARSZone> Zone;

	/**
	 * The barricaded window in front of this point.
	 *
	 * One spawn behind one window is the genre's arrangement, and it is what lets an enemy be
	 * told where to go rather than having to work it out: no registry, no nearest-barricade
	 * search, no guessing from a failed path. A point with none set releases enemies that head
	 * straight for a player, which is what the open spawners in the test level do.
	 */
	UPROPERTY(EditInstanceOnly, Category = "RS|Spawning")
	TObjectPtr<ARSBarricade> Barricade;

	/**
	 * Minimum interval between two releases from this point.
	 *
	 * Without it a burst puts several enemies inside one another at the same coordinates, and the
	 * physics that pushes them apart is far more expensive than waiting a second.
	 */
	UPROPERTY(EditAnywhere, Category = "RS|Spawning", meta = (ForceUnits = s, ClampMin = 0.0))
	float SpawnCooldown = 1.0f;

	/** Radius checked for pawns before the point is offered. Roughly one enemy capsule. */
	UPROPERTY(EditAnywhere, Category = "RS|Spawning", meta = (ForceUnits = cm, ClampMin = 0.0))
	float ClearanceRadius = 45.0f;

	UPROPERTY(EditAnywhere, Category = "RS|Spawning")
	bool bProjectToNavigation = true;

	/** How far up and down to look for the navigation mesh under the point. */
	UPROPERTY(EditAnywhere, Category = "RS|Spawning", meta = (ForceUnits = cm, ClampMin = 1.0, EditCondition = "bProjectToNavigation"))
	float NavigationProjectionExtent = 200.0f;

private:
#if WITH_EDITORONLY_DATA
	/** Which way an enemy faces when it arrives. Editor only: the actor is invisible in game. */
	UPROPERTY()
	TObjectPtr<UArrowComponent> ArrowComponent;

	UPROPERTY()
	TObjectPtr<UBillboardComponent> BillboardComponent;
#endif

	/** World seconds of the last release. Negative infinity so a fresh point is never on cooldown. */
	double LastSpawnTime = -TNumericLimits<double>::Max();
};
