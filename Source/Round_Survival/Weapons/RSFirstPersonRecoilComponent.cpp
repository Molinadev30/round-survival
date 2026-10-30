// Round Survival

#include "Weapons/RSFirstPersonRecoilComponent.h"

#include "Core/RSPlayerCharacter.h"

namespace
{
	/** One random value per axis inside [Min, Max]. */
	FVector RandomKick(const FVector& Min, const FVector& Max)
	{
		return FVector(FMath::FRandRange(Min.X, Max.X), FMath::FRandRange(Min.Y, Max.Y), FMath::FRandRange(Min.Z, Max.Z));
	}

	FVector ClampSymmetric(const FVector& Value, const FVector& Limit)
	{
		return FVector(FMath::Clamp(Value.X, -Limit.X, Limit.X), FMath::Clamp(Value.Y, -Limit.Y, Limit.Y), FMath::Clamp(Value.Z, -Limit.Z, Limit.Z));
	}

	/** Rotators as pitch, yaw, roll vectors, which is what the spring works on. */
	FVector ToVector(const FRotator& Rotator)
	{
		return FVector(Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
	}
}

URSFirstPersonRecoilComponent::URSFirstPersonRecoilComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Off until a shot kicks it; it switches itself off again once everything is back at rest.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Cosmetic and local: nothing here is replicated or read by gameplay.
	SetIsReplicatedByDefault(false);
}

void URSFirstPersonRecoilComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ARSPlayerCharacter* Character = Cast<ARSPlayerCharacter>(GetOwner()))
	{
		Viewmodel = Character->GetFirstPersonMesh();
	}

	// The viewmodel's animation reads the kick while it ticks, so resolve it first: otherwise the
	// arms would show the previous frame's kick.
	if (USceneComponent* Mesh = Viewmodel.Get())
	{
		Mesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
	}
}

void URSFirstPersonRecoilComponent::AddRecoil(const FRSWeaponRecoil& Recoil)
{
	if (!Viewmodel.IsValid())
	{
		return;
	}

	Settings = Recoil;

	TargetLocation = ClampSymmetric(TargetLocation + RandomKick(Settings.LocationKickMin, Settings.LocationKickMax), Settings.LocationLimit);
	TargetRotation = ClampSymmetric(TargetRotation + RandomKick(ToVector(Settings.RotationKickMin), ToVector(Settings.RotationKickMax)),
		ToVector(Settings.RotationLimit));

	SetComponentTickEnabled(true);
}

void URSFirstPersonRecoilComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Viewmodel.IsValid() || DeltaTime <= 0.0f)
	{
		return;
	}

	// The kick drains back to rest on its own...
	TargetLocation = FMath::VInterpTo(TargetLocation, FVector::ZeroVector, DeltaTime, Settings.ReturnSpeed);
	TargetRotation = FMath::VInterpTo(TargetRotation, FVector::ZeroVector, DeltaTime, Settings.ReturnSpeed);

	// ...and the viewmodel chases it through a spring, which overshoots a little: the bounce.
	CurrentLocation = UKismetMathLibrary::VectorSpringInterp(CurrentLocation, TargetLocation, LocationSpring,
		Settings.LocationStiffness, Settings.LocationDamping, DeltaTime, Settings.LocationMass, Settings.LocationTargetVelocity);
	CurrentRotation = UKismetMathLibrary::VectorSpringInterp(CurrentRotation, TargetRotation, RotationSpring,
		Settings.RotationStiffness, Settings.RotationDamping, DeltaTime, Settings.RotationMass, Settings.RotationTargetVelocity);

	// Back at rest and no longer moving: stop ticking until the next shot.
	constexpr float RestTolerance = 0.001f;
	const bool bAtRest = TargetLocation.IsNearlyZero(RestTolerance) && TargetRotation.IsNearlyZero(RestTolerance)
		&& CurrentLocation.IsNearlyZero(RestTolerance) && CurrentRotation.IsNearlyZero(RestTolerance)
		&& LocationSpring.Velocity.IsNearlyZero(RestTolerance) && RotationSpring.Velocity.IsNearlyZero(RestTolerance);

	if (bAtRest)
	{
		CurrentLocation = FVector::ZeroVector;
		CurrentRotation = FVector::ZeroVector;
		LocationSpring.Reset();
		RotationSpring.Reset();

		SetComponentTickEnabled(false);
	}
}
