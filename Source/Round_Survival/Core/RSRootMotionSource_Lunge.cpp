// Round Survival

#include "Core/RSRootMotionSource_Lunge.h"

#include "GameFramework/Character.h"

FRSRootMotionSource_Lunge::FRSRootMotionSource_Lunge()
{
	// Overrides the velocity in the plane and keeps its height, so gravity still acts in the air.
	AccumulateMode = ERootMotionAccumulateMode::Override;
	Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
}

FRootMotionSource* FRSRootMotionSource_Lunge::Clone() const
{
	return new FRSRootMotionSource_Lunge(*this);
}

bool FRSRootMotionSource_Lunge::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	// Safe: the base has checked the script structs are the same.
	const FRSRootMotionSource_Lunge* OtherLunge = static_cast<const FRSRootMotionSource_Lunge*>(Other);
	return FMath::IsNearlyEqual(StopDistance, OtherLunge->StopDistance) && FMath::IsNearlyEqual(MaxSpeed, OtherLunge->MaxSpeed);
}

bool FRSRootMotionSource_Lunge::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::MatchesAndHasSameState(Other))
	{
		return false;
	}

	const FRSRootMotionSource_Lunge* OtherLunge = static_cast<const FRSRootMotionSource_Lunge*>(Other);
	return TargetLocation.Equals(OtherLunge->TargetLocation) && TargetVelocity.Equals(OtherLunge->TargetVelocity);
}

bool FRSRootMotionSource_Lunge::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	if (!FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup))
	{
		return false;
	}

	const FRSRootMotionSource_Lunge* OtherLunge = static_cast<const FRSRootMotionSource_Lunge*>(SourceToTakeStateFrom);
	TargetLocation = OtherLunge->TargetLocation;
	TargetVelocity = OtherLunge->TargetVelocity;
	return true;
}

void FRSRootMotionSource_Lunge::PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();

	if (MovementTickTime > UE_SMALL_NUMBER)
	{
		// The time left until the blow lands, never less than this step: the last step closes what is left.
		const float TimeLeft = FMath::Max(GetDuration() - GetTime(), MovementTickTime);

		const FVector Current = Character.GetActorLocation();
		const FVector Ahead = TargetLocation + TargetVelocity * TimeLeft;

		FVector Velocity = FVector::ZeroVector;
		if (FVector::DistSquared2D(Current, TargetLocation) > FMath::Square(StopDistance))
		{
			// Towards where the enemy will be, to a point StopDistance short of it, in the time left.
			const FVector ToAhead = FVector(Ahead.X - Current.X, Ahead.Y - Current.Y, 0.0f);
			const float Travel = FMath::Max(0.0f, static_cast<float>(ToAhead.Size()) - StopDistance);
			Velocity = ToAhead.GetSafeNormal() * FMath::Min(Travel / TimeLeft, MaxSpeed);
		}

		RootMotionParams.Set(FTransform(Velocity));
	}

	SetTime(GetTime() + SimulationTime);
}

bool FRSRootMotionSource_Lunge::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!FRootMotionSource::NetSerialize(Ar, Map, bOutSuccess))
	{
		return false;
	}

	Ar << TargetLocation;
	Ar << TargetVelocity;
	Ar << StopDistance;
	Ar << MaxSpeed;

	bOutSuccess = true;
	return true;
}

UScriptStruct* FRSRootMotionSource_Lunge::GetScriptStruct() const
{
	return FRSRootMotionSource_Lunge::StaticStruct();
}

FString FRSRootMotionSource_Lunge::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FRSRootMotionSource_Lunge %s"), LocalID, *InstanceName.GetPlainNameString());
}
