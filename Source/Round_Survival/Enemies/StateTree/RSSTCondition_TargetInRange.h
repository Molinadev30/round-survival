// Round Survival

#pragma once

#include "StateTreeConditionBase.h"

#include "RSSTCondition_TargetInRange.generated.h"

class ARSZombieController;

USTRUCT()
struct FRSSTCondition_TargetInRangeInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller, which owns the targeting. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** Horizontal distance, capsule centre to capsule centre, inside which this passes. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = cm))
	float Range = 190.0f;

	/**
	 * Also require an unobstructed line to the target.
	 *
	 * Distance alone says nothing about whether the target can be reached: a player standing a
	 * metre away on the far side of a wall is close, and an enemy that attacked on distance alone
	 * would stand there swinging at masonry instead of walking around. This is the same question
	 * the swing itself asks before it deals damage, asked early enough to keep walking instead.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bRequireLineOfSight = true;

	/** Inverts the test, for the states that want to know the target has got away. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/**
 * True while the enemy's current target is close enough, and visible enough, to be worth swinging
 * at.
 *
 * Deliberately a little more generous in distance than the reach of the swing itself: an enemy
 * that stopped exactly on the attack threshold would sit there failing this test by a centimetre.
 * The gap between the two numbers is intentional, not an inconsistency to be tidied away.
 *
 * Reads the target from the controller's targeting component, so it can never disagree with what
 * the chase is walking towards.
 */
USTRUCT(meta = (DisplayName = "Target In Range", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTCondition_TargetInRange : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTCondition_TargetInRangeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
