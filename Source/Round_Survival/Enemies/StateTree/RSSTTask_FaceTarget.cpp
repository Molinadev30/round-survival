// Round Survival

#include "Enemies/StateTree/RSSTTask_FaceTarget.h"

#include "Enemies/RSEnemyDebug.h"
#include "Enemies/RSZombieController.h"
#include "Enemies/RSZombieTargetingComponent.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

FRSSTTask_FaceTarget::FRSSTTask_FaceTarget()
{
	bShouldCallTick = true;

	// The state it sits on is finished by its other task — the attack, the barricade, whatever it
	// is. This one just turns the body while that happens.
#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
#endif
}

EStateTreeRunStatus FRSSTTask_FaceTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	Context.GetInstanceData(*this).ElapsedTime = 0.0f;

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FRSSTTask_FaceTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;

	const ARSZombieController* Controller = InstanceData.AIController;
	APawn* ControlledPawn = Controller ? Controller->GetPawn() : nullptr;
	const URSZombieTargetingComponent* Targeting = Controller ? Controller->GetTargetingComponent() : nullptr;
	const AActor* Target = Targeting ? Targeting->GetTarget() : nullptr;

	// This is the only task that ticks through the whole attack, so it is also where the attack
	// gets labelled — before the wind-up check below, or the label would vanish halfway through
	// the swing and a committed enemy would look like one drawing nothing.
	RSEnemyDebug::DrawEnemyState(ControlledPawn, Target, TEXT("Attack"),
		Targeting ? Targeting->GetTargetPathDistance() : -1.0f);

	if (InstanceData.TurnDuration > 0.0f && InstanceData.ElapsedTime > InstanceData.TurnDuration)
	{
		// Past the wind-up. Still running, but no longer aiming: the swing is committed.
		return EStateTreeRunStatus::Running;
	}

	if (!ControlledPawn || !Target || InstanceData.TurnRate <= 0.0f || DeltaTime <= 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	const FVector ToTarget = (Target->GetActorLocation() - ControlledPawn->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return EStateTreeRunStatus::Running;
	}

	// Yaw only and rate limited. Snapping to face would read as a turret rather than a body, and
	// it would take the dodge away from anyone quick enough to get around the side.
	FRotator Current = ControlledPawn->GetActorRotation();
	Current.Pitch = 0.0f;
	Current.Roll = 0.0f;

	const FRotator Desired(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	ControlledPawn->SetActorRotation(FMath::RInterpConstantTo(Current, Desired, DeltaTime, InstanceData.TurnRate));

	return EStateTreeRunStatus::Running;
}
