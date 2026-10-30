// Round Survival

#pragma once

#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"

#include "RSSeveredPiece.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * A part that has come off an enemy, flying and then lying where it fell.
 *
 * Cosmetic and local: never replicated, each machine launches its own from the cut it was told
 * about, and nothing in the game reads it. It collides with the world and with other bodies, and
 * with nothing that matters — not pawns, not shots, not the camera.
 *
 * Launched in the pose the part had on the body: each of its physics bodies starts where the same
 * bone was, so the piece leaves the stump rather than appearing in its reference pose beside it.
 * Pooled by URSSeveredPieceSubsystem.
 */
UCLASS(NotBlueprintable)
class ROUND_SURVIVAL_API ARSSeveredPiece : public AActor
{
	GENERATED_BODY()

public:
	ARSSeveredPiece(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Throws the piece: its mesh in the pose `Source` has it, every body moving at `Velocity`.
	 * `TrailEffect`, if any, rides on the cut end until the piece goes, `Lifetime` seconds later.
	 */
	void Launch(USkeletalMesh* PieceMesh, const USkeletalMeshComponent& Source, const FVector& Velocity,
		UNiagaraSystem* TrailEffect, float Lifetime);

	/** Out of the world, ready to be thrown again. */
	void Release();

	bool IsLaunched() const { return bLaunched; }

	/** World time of the last launch, so the pool can take the oldest. */
	double GetLaunchTime() const { return LaunchTime; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "RS|Enemy")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Trail;

private:
	FTimerHandle LifetimeHandle;
	double LaunchTime = 0.0;
	bool bLaunched = false;
};

/**
 * The pieces in one world, a fixed number at most.
 *
 * Made as they are first needed, and reused after: a piece that has gone is thrown again, and when
 * all of them are out the oldest is taken back for the newest cut. URSDismembermentSettings sets
 * how many and for how long.
 */
UCLASS()
class ROUND_SURVIVAL_API URSSeveredPieceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static URSSeveredPieceSubsystem* Get(const UObject* WorldContextObject);

	/** Throws a piece, as ARSSeveredPiece::Launch. Null when pieces are turned off. */
	ARSSeveredPiece* Launch(USkeletalMesh* PieceMesh, const USkeletalMeshComponent& Source, const FVector& Velocity,
		UNiagaraSystem* TrailEffect);

protected:
	//~USubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~End of USubsystem interface

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ARSSeveredPiece>> Pieces;
};
