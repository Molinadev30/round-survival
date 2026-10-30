// Round Survival

#pragma once

#include "Tasks/StateTreeAITask.h"

#include "RSSTTask_FaceLocation.generated.h"

class ARSZombieController;

USTRUCT()
struct FRSSTTask_FaceLocationInstanceData
{
	GENERATED_BODY()

	/** Bound by the schema to the enemy's own controller. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ARSZombieController> AIController = nullptr;

	/** What to turn towards. Bound, so the state decides what "towards" means. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector FaceLocation = FVector::ZeroVector;

	/**
	 * Degrees to turn past facing the point, positive to the enemy's right. Zero faces it square
	 * on; the reach binds its slot's offset here so the animation's arm lines up with its gap.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ForceUnits = deg))
	float YawOffset = 0.0f;

	/** Degrees per second. Matched to the turn the enemy uses on a player, for one body language. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ForceUnits = "deg/s"))
	float TurnRate = 240.0f;

	/** Label drawn over the enemy by the AI debug draw while this state runs. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FString DebugLabel = TEXT("Tear");
};

/**
 * Turns the enemy on the spot towards a point.
 *
 * `Face Target` turns towards whatever the targeting component is chasing, which is always a
 * player. This one turns towards a place, which is what a state fighting a piece of the level
 * needs: the window is not a target, nothing targets it, and an enemy that arrived at its slot
 * from the side would otherwise tear boards off while facing down the street.
 *
 * Both exist for the same underlying reason — the character movement component only turns a pawn
 * that is walking somewhere, and this one has arrived and stopped. So it only turns a pawn that is
 * standing, and leaves a walking one to the movement component: in the queue at a full window it
 * runs beside the walk to the queue's spot, and turns the enemy to the window once it is there.
 *
 * Never completes and is not counted towards its state's completion: the attack or the walk beside
 * it is what ends the state.
 */
USTRUCT(meta = (DisplayName = "Face Location", Category = "Round Survival|Enemy"))
struct ROUND_SURVIVAL_API FRSSTTask_FaceLocation : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRSSTTask_FaceLocationInstanceData;

	FRSSTTask_FaceLocation();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Movement"); }
#endif // WITH_EDITOR
};
