// Round Survival

#include "Enemies/RSEnemyDebug.h"

#include "Enemies/RSDismembermentComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

namespace RSEnemyDebug
{
#if ENABLE_DRAW_DEBUG
	static TAutoConsoleVariable<int32> CVarEnemyDebug(
		TEXT("RS.Enemy.Debug"),
		0,
		TEXT("Draws enemy AI reasoning: melee sweep, frontal cone, line of sight, current target and attack range.\n")
		TEXT("  0: off (default)\n")
		TEXT("  1: on"),
		ECVF_Cheat);
#endif

	bool IsEnabled()
	{
#if ENABLE_DRAW_DEBUG
		return CVarEnemyDebug.GetValueOnAnyThread() > 0;
#else
		return false;
#endif
	}

	FColor PassColor()
	{
		return FColor::Green;
	}

	FColor FailColor()
	{
		return FColor(255, 80, 80);
	}

	void DrawGroundCone(const UWorld* World, const FVector& Origin, const FVector& Direction,
		float Radius, float HalfAngleDegrees, const FColor& Color, float Lifetime)
	{
#if ENABLE_DRAW_DEBUG
		if (!World || Radius <= 0.0f)
		{
			return;
		}

		const FVector Forward = Direction.GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			return;
		}

		// Flat on the ground rather than a true 3D cone: the test itself is horizontal, and a cone
		// drawn in three dimensions would suggest a height limit the code does not have.
		const int32 Segments = 16;
		const float HalfAngle = FMath::DegreesToRadians(HalfAngleDegrees);
		const float Step = (HalfAngle * 2.0f) / Segments;
		const FVector Base = Origin - FVector(0.0f, 0.0f, 80.0f);

		FVector Previous = Base + Forward.RotateAngleAxis(-HalfAngleDegrees, FVector::UpVector) * Radius;
		DrawDebugLine(World, Base, Previous, Color, /*bPersistentLines=*/ false, Lifetime);

		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float AngleDegrees = FMath::RadiansToDegrees(-HalfAngle + Step * Index);
			const FVector Point = Base + Forward.RotateAngleAxis(AngleDegrees, FVector::UpVector) * Radius;

			DrawDebugLine(World, Previous, Point, Color, /*bPersistentLines=*/ false, Lifetime);
			Previous = Point;
		}

		DrawDebugLine(World, Base, Previous, Color, /*bPersistentLines=*/ false, Lifetime);
#endif
	}

	void DrawEnemyState(const APawn* Pawn, const AActor* Target, const TCHAR* StateName, float PathDistance)
	{
#if ENABLE_DRAW_DEBUG
		const UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
		if (!IsEnabled() || !World)
		{
			return;
		}

		FString Label = FString::Printf(TEXT("%s\n"), StateName);

		if (Target)
		{
			const FVector From = Pawn->GetActorLocation();
			DrawDebugLine(World, From, Target->GetActorLocation(), FColor::Cyan, /*bPersistentLines=*/ false, /*LifeTime=*/ -1.0f);

			// Straight line and travel distance side by side: where they disagree is where a horde
			// splits differently from how it looks like it should.
			Label += FString::Printf(TEXT("%s\ndirect %.0f cm\n"), *GetNameSafe(Target),
				FVector::Dist2D(From, Target->GetActorLocation()));
			Label += PathDistance >= 0.0f
				? FString::Printf(TEXT("path %.0f cm"), PathDistance)
				: FString(TEXT("path not measured (sole candidate)"));
		}
		else
		{
			Label += TEXT("no target");
		}

		// What it has lost, and how close the rest is to going.
		if (const URSDismembermentComponent* Dismemberment = Pawn->FindComponentByClass<URSDismembermentComponent>())
		{
			const FString Parts = Dismemberment->GetDebugString();
			if (!Parts.IsEmpty())
			{
				Label += TEXT("\n") + Parts;
			}
		}

		DrawDebugString(World, FVector(0.0f, 0.0f, 110.0f), Label, const_cast<APawn*>(Pawn), FColor::Cyan,
			/*Duration=*/ 0.0f, /*bDrawShadow=*/ true);
#endif
	}
}
