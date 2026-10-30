// Round Survival

#include "Enemies/StateTree/RSSTCondition_TargetInRange.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Enemies/RSEnemyDebug.h"
#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "Enemies/RSZombieTargetingComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/RSBarricade.h"
#include "StateTreeExecutionContext.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

bool FRSSTCondition_TargetInRange::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const ARSZombieController* Controller = InstanceData.AIController;
	APawn* ControlledPawn = Controller ? Controller->GetPawn() : nullptr;
	const URSZombieTargetingComponent* Targeting = Controller ? Controller->GetTargetingComponent() : nullptr;
	APawn* Target = Targeting ? Targeting->GetTarget() : nullptr;

	// No target is not in range, however the test is phrased: inverted, it should read as "the
	// thing I was going to attack is not there", which is exactly true when there is nothing.
	if (!ControlledPawn || !Target)
	{
		return InstanceData.bInvert;
	}

	// Horizontal only. A target standing on a crate is as attackable as one on the floor, and the
	// chase that got here measured the same way.
	const FVector Origin = ControlledPawn->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();

	bool bPasses = FVector::Dist2D(Origin, TargetLocation) <= InstanceData.Range;

	if (bPasses && InstanceData.bRequireLineOfSight)
	{
		const UWorld* World = ControlledPawn->GetWorld();

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RSTargetInRange), /*bTraceComplex=*/ false, ControlledPawn);
		QueryParams.AddIgnoredActor(Target);

		// The window this enemy came in through is not a wall to it. Only that one, so reaching
		// through the boards it is holding stays possible while swinging through anything else in
		// the level does not. The swing itself makes the same exception, in
		// `URSGameplayAbility_ZombieMelee`: deciding to attack and landing the attack have to
		// agree about what counts as cover, or an enemy commits to a montage that cannot connect.
		const ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(ControlledPawn);
		ARSBarricade* EntryBarricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;
		if (IsValid(EntryBarricade))
		{
			QueryParams.AddIgnoredActor(EntryBarricade);
		}

		// What either of them is carrying is not a wall either.
		TArray<AActor*> AttachedActors;
		Target->GetAttachedActors(AttachedActors);
		QueryParams.AddIgnoredActors(AttachedActors);

		AttachedActors.Reset();
		ControlledPawn->GetAttachedActors(AttachedActors);
		QueryParams.AddIgnoredActors(AttachedActors);

		FHitResult Blocker;
		bPasses = !World || !World->LineTraceSingleByChannel(Blocker, Origin, TargetLocation, ECC_Visibility, QueryParams);
	}

#if ENABLE_DRAW_DEBUG
	if (RSEnemyDebug::IsEnabled())
	{
		// The ring on the floor is the whole decision in one shape: green means the enemy would
		// attack from here, red means it keeps walking.
		DrawDebugCircle(ControlledPawn->GetWorld(), Origin - FVector(0.0f, 0.0f, 88.0f), InstanceData.Range, 32,
			bPasses ? RSEnemyDebug::PassColor() : RSEnemyDebug::FailColor(), /*bPersistentLines=*/ false, /*LifeTime=*/ -1.0f,
			/*DepthPriority=*/ 0, /*Thickness=*/ 1.0f, FVector(1, 0, 0), FVector(0, 1, 0), /*bDrawAxis=*/ false);
	}
#endif

	return InstanceData.bInvert ? !bPasses : bPasses;
}
