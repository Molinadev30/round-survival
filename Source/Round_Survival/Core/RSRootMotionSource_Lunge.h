// Round Survival

#pragma once

#include "GameFramework/RootMotionSource.h"

#include "RSRootMotionSource_Lunge.generated.h"

/**
 * The melee lunge: carries the player towards an enemy so the blow lands from just in front of it.
 *
 * Every step it aims for where the enemy will be when the blow lands — its location plus its velocity
 * times the time left — stopping StopDistance short of it, and sets the speed that gets there exactly
 * on time, never more than MaxSpeed. It is horizontal only: the source overrides the velocity in the
 * plane and leaves its height alone (IgnoreZAccumulate), so gravity keeps acting and the player never
 * floats up to or digs down towards an enemy on another level.
 *
 * A root motion source rather than anything of our own because the movement component already carries
 * these in every saved move and in its corrections, and replicates them to everybody else's copy: the
 * lunge is predicted and reconciled like any other movement. The target is updated from outside every
 * frame (URSAbilityTask_Lunge), on the striker and on the server alike.
 */
USTRUCT()
struct ROUND_SURVIVAL_API FRSRootMotionSource_Lunge : public FRootMotionSource
{
	GENERATED_BODY()

	FRSRootMotionSource_Lunge();

	virtual ~FRSRootMotionSource_Lunge() = default;

	/** Where the enemy is now. */
	UPROPERTY()
	FVector TargetLocation = FVector::ZeroVector;

	/** How fast it is moving, to lead it by the time left. */
	UPROPERTY()
	FVector TargetVelocity = FVector::ZeroVector;

	/** How far short of the enemy the lunge stops, centre to centre. Inside it, the lunge does nothing. */
	UPROPERTY()
	float StopDistance = 0.0f;

	UPROPERTY()
	float MaxSpeed = 0.0f;

	//~FRootMotionSource interface
	virtual FRootMotionSource* Clone() const override;
	virtual bool Matches(const FRootMotionSource* Other) const override;
	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;
	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;
	virtual void PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent) override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	//~End of FRootMotionSource interface
};

template<>
struct TStructOpsTypeTraits<FRSRootMotionSource_Lunge> : public TStructOpsTypeTraitsBase2<FRSRootMotionSource_Lunge>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
