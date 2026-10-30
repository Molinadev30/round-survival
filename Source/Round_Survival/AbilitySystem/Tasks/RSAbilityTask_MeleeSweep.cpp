// Round Survival

#include "AbilitySystem/Tasks/RSAbilityTask_MeleeSweep.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "CollisionQueryParams.h"
#include "Enemies/RSEnemyDebug.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

URSAbilityTask_MeleeSweep::URSAbilityTask_MeleeSweep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A damage window lasts a handful of frames. Sampling it once would make a hit depend on which
	// frame the sample landed on, which is the kind of bug that only shows up on a slower machine.
	bTickingTask = true;
}

URSAbilityTask_MeleeSweep* URSAbilityTask_MeleeSweep::MeleeSweep(UGameplayAbility* OwningAbility, float Duration, float Range,
	float SweepRadius, float HalfAngleDegrees, const TArray<AActor*>& IgnoredActors, bool bDrawDebug)
{
	URSAbilityTask_MeleeSweep* Task = NewAbilityTask<URSAbilityTask_MeleeSweep>(OwningAbility);

	Task->Duration = FMath::Max(0.0f, Duration);
	Task->Range = FMath::Max(0.0f, Range);
	Task->SweepRadius = FMath::Max(1.0f, SweepRadius);
	Task->HalfAngleDegrees = FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f);
	Task->CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Task->HalfAngleDegrees));
	Task->bDrawDebug = bDrawDebug;

	Task->IgnoredActors.Reserve(IgnoredActors.Num());
	for (AActor* Actor : IgnoredActors)
	{
		Task->IgnoredActors.Add(Actor);
	}

	return Task;
}

void URSAbilityTask_MeleeSweep::SetFixedFrame(const FVector& InOrigin, const FVector& InForward)
{
	bHasFixedFrame = true;
	FixedOrigin = InOrigin;
	FixedForward = InForward.GetSafeNormal2D();
}

bool URSAbilityTask_MeleeSweep::ShouldDraw() const
{
	return bDrawDebug || RSEnemyDebug::IsEnabled();
}

void URSAbilityTask_MeleeSweep::Activate()
{
	TimeRemaining = Duration;

	// A zero length window would otherwise never be sampled at all: the first tick arrives after
	// the window has already expired.
	FHitResult Hit;
	if (FindTarget(Hit))
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnHit.Broadcast(Hit);
		}
		EndTask();
	}
}

void URSAbilityTask_MeleeSweep::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	FHitResult Hit;
	if (FindTarget(Hit))
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnHit.Broadcast(Hit);
		}
		EndTask();
		return;
	}

	TimeRemaining -= DeltaTime;
	if (TimeRemaining <= 0.0f)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnMissed.Broadcast();
		}
		EndTask();
	}
}

bool URSAbilityTask_MeleeSweep::FindTarget(FHitResult& OutHit) const
{
	const AActor* Avatar = GetAvatarActor();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	const FVector Origin = bHasFixedFrame ? FixedOrigin : Avatar->GetActorLocation();
	const FVector Forward = bHasFixedFrame ? FixedForward : Avatar->GetActorForwardVector().GetSafeNormal2D();
	const FVector End = Origin + Forward * Range;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RSMeleeSweep), /*bTraceComplex=*/ false, Avatar);
	QueryParams.AddIgnoredActors(IgnoredActors);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, Origin, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(SweepRadius), QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!Candidate || Candidate == Avatar)
		{
			continue;
		}

		// Only things with an ability system can take damage from this. Hitting the world is what
		// the line of sight trace below is for, not a target in its own right.
		if (!UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate))
		{
			continue;
		}

		const FVector ToTarget = (Candidate->GetActorLocation() - Origin).GetSafeNormal2D();
		if (FVector::DotProduct(Forward, ToTarget) < CosHalfAngle)
		{
			// Behind, or far enough to the side that the animation never reached them.
			continue;
		}

		if (!HasLineOfSight(Origin, Candidate))
		{
			continue;
		}

		DrawSweep(World, Origin, End, Forward, /*bConnected=*/ true);

		OutHit = Hit;
		return true;
	}

	DrawSweep(World, Origin, End, Forward, /*bConnected=*/ false);

	return false;
}

void URSAbilityTask_MeleeSweep::DrawSweep(const UWorld* World, const FVector& Origin, const FVector& End,
	const FVector& Forward, bool bConnected) const
{
#if ENABLE_DRAW_DEBUG
	if (!ShouldDraw() || !World)
	{
		return;
	}

	// Long enough to read a swing that lasted a quarter of a second, short enough not to smear
	// one swing over the next.
	const float Lifetime = 0.35f;
	const FColor Color = bConnected ? RSEnemyDebug::PassColor() : RSEnemyDebug::FailColor();

	DrawDebugCapsule(World, (Origin + End) * 0.5f, (End - Origin).Size() * 0.5f, SweepRadius,
		FRotationMatrix::MakeFromZ(Forward).ToQuat(), Color, /*bPersistentLines=*/ false, Lifetime);

	// The cone is the filter people get wrong when a swing "should" have hit, so it is drawn with
	// the swing rather than left to be imagined.
	RSEnemyDebug::DrawGroundCone(World, Origin, Forward, Range, HalfAngleDegrees, Color, Lifetime);
#endif
}

bool URSAbilityTask_MeleeSweep::HasLineOfSight(const FVector& Origin, const AActor* Target) const
{
	const AActor* Avatar = GetAvatarActor();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World || !Target)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RSMeleeLineOfSight), /*bTraceComplex=*/ false, Avatar);
	QueryParams.AddIgnoredActors(IgnoredActors);
	QueryParams.AddIgnoredActor(Target);

	// What either of them is carrying is not cover. A weapon actor attached to the target sits
	// right in front of its chest, and left in, it would shield its owner from every swing.
	TArray<AActor*> AttachedActors;
	Target->GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);

	AttachedActors.Reset();
	Avatar->GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);

	FHitResult Blocker;
	const bool bBlocked = World->LineTraceSingleByChannel(Blocker, Origin, Target->GetActorLocation(), ECC_Visibility, QueryParams);

#if ENABLE_DRAW_DEBUG
	if (ShouldDraw())
	{
		const float Lifetime = 0.35f;
		DrawDebugLine(World, Origin, Target->GetActorLocation(),
			bBlocked ? RSEnemyDebug::FailColor() : RSEnemyDebug::PassColor(), /*bPersistentLines=*/ false, Lifetime);

		if (bBlocked)
		{
			// Where the swing stopped, and on what. Usually the answer to "why did that miss".
			DrawDebugPoint(World, Blocker.ImpactPoint, 12.0f, RSEnemyDebug::FailColor(), /*bPersistentLines=*/ false, Lifetime);
			DrawDebugString(World, Blocker.ImpactPoint + FVector(0.0f, 0.0f, 20.0f),
				GetNameSafe(Blocker.GetActor()), /*TestBaseActor=*/ nullptr, RSEnemyDebug::FailColor(), Lifetime, /*bDrawShadow=*/ true);
		}
	}
#endif

	return !bBlocked;
}
