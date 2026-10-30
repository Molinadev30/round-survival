// Round Survival

#include "Enemies/StateTree/RSSTTask_FaceLocation.h"

#include "Enemies/RSEnemyDebug.h"
#include "Enemies/RSZombieController.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

namespace RSFaceLocation
{
	/** Below this the pawn is standing, give or take a shove from the crowd around it. */
	static constexpr float StandingSpeed = 10.0f;
}

FRSSTTask_FaceLocation::FRSSTTask_FaceLocation()
{
	bShouldCallTick = true;

	// The state is finished by whatever it is really doing — the swing at the window. This one
	// just turns the body while that happens.
#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
#endif
}

EStateTreeRunStatus FRSSTTask_FaceLocation::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const ARSZombieController* Controller = InstanceData.AIController;
	APawn* ControlledPawn = Controller ? Controller->GetPawn() : nullptr;

	RSEnemyDebug::DrawEnemyState(ControlledPawn, /*Target=*/ nullptr, *InstanceData.DebugLabel, /*PathDistance=*/ -1.0f);

	if (!ControlledPawn || InstanceData.TurnRate <= 0.0f || DeltaTime <= 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	// A pawn still walking is the movement component's to turn, towards where it walks. Turning it
	// here as well would fight that every frame. In the queue this runs beside the walk to the
	// queue's spot, and takes over once the enemy stands there.
	if (ControlledPawn->GetVelocity().SizeSquared2D() > FMath::Square(RSFaceLocation::StandingSpeed))
	{
		return EStateTreeRunStatus::Running;
	}

	const FVector ToPoint = (InstanceData.FaceLocation - ControlledPawn->GetActorLocation()).GetSafeNormal2D();
	if (ToPoint.IsNearlyZero())
	{
		// Standing on the point, or the point was never written. Turning towards nothing would
		// spin the pawn to whatever a zero vector happens to face.
		return EStateTreeRunStatus::Running;
	}

	// Yaw only and rate limited, the same turn a swing at a player uses: a body that snaps round
	// to face a window reads as a machine, and these are not machines.
	FRotator Current = ControlledPawn->GetActorRotation();
	Current.Pitch = 0.0f;
	Current.Roll = 0.0f;

	const FRotator Desired(0.0f, ToPoint.Rotation().Yaw + InstanceData.YawOffset, 0.0f);
	ControlledPawn->SetActorRotation(FMath::RInterpConstantTo(Current, Desired, DeltaTime, InstanceData.TurnRate));

	return EStateTreeRunStatus::Running;
}
