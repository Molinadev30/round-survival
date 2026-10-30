// Round Survival

#pragma once

#include "Tasks/StateTreeAITask.h"

#include "RSSTTask_FaceTarget.generated.h"

class AActor;
class APawn;
class ARSZombieController;

USTRUCT()
struct FRSSTTask_FaceTargetInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** Degrees per second. Slow enough to be outrun by a player walking a circle, on purpose. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = "deg/s"))
	float TurnRate = 240.0f;

	/**
	 * How long into the state the enemy keeps turning. Zero means for as long as the state lasts.
	 *
	 * On an attack this is the wind-up: the enemy aims at where you are as it raises its arms and
	 * is then committed, so stepping around it mid-swing is a real dodge rather than something the
	 * animation quietly corrects for.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = s))
	float TurnDuration = 0.0f;

	UPROPERTY()
	float ElapsedTime = 0.0f;
};

/**
 * Turns the enemy on the spot towards whatever it is targeting.
 *
 * A task of its own rather than part of the chase, because the state that needs it most is the one
 * where the pawn is not moving at all: the character movement component only turns a pawn while it
 * is walking somewhere, so a standing enemy would otherwise keep facing the spot its target used
 * to be in.
 *
 * Never completes, and is not counted towards its state's completion — it runs alongside whatever
 * the state is really doing.
 */
USTRUCT(meta = (DisplayName = "Face Target", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTTask_FaceTarget : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTTask_FaceTargetInstanceData;

	FRSSTTask_FaceTarget();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Movement"); }
#endif // WITH_EDITOR
};
