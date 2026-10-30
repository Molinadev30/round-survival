// Round Survival

#pragma once

#include "Components/ActorComponent.h"
#include "Kismet/KismetMathLibrary.h"

#include "RSFirstPersonRecoilComponent.generated.h"

class USceneComponent;

/**
 * How hard a weapon kicks the viewmodel. Cosmetic: spread and aim are not touched.
 *
 * Each shot adds a random kick inside the Min / Max ranges. The total is clamped to the limits,
 * pulled back towards rest at ReturnSpeed, and the viewmodel follows it through a spring, which is
 * where the bounce comes from. Because the return drains most of a kick before the next shot lands,
 * automatic fire settles at a steady depth instead of piling up.
 *
 * Every shot kicks the same: a per-burst ramp made a long burst shove five times harder.
 *
 * Offsets are along the viewmodel mesh's own axes, as the animations author it: Y runs along
 * the barrel, so a negative Y kick pushes the weapon back into the shoulder, and Roll — rotation
 * about that axis as the mesh is oriented — is what lifts the muzzle. Rotation turns the weapon
 * about its own grip.
 */
USTRUCT(BlueprintType)
struct FRSWeaponRecoil
{
	GENERATED_BODY()

	/** Per-shot kick in cm. A zero range on every axis turns the recoil off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Kick")
	FVector LocationKickMin = FVector(-0.25, -7.5, -0.25);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Kick")
	FVector LocationKickMax = FVector(0.25, -2.5, 0.25);

	/** Per-shot kick in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Kick")
	FRotator RotationKickMin = FRotator(-1.0, -2.5, -1.25);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Kick")
	FRotator RotationKickMax = FRotator(1.0, 2.5, 1.25);

	/** How far the accumulated kick may go on each axis, either way. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Kick", meta = (ForceUnits = cm))
	FVector LocationLimit = FVector(5.0, 50.0, 0.5);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Kick")
	FRotator RotationLimit = FRotator(25.0, 1.0, 100.0);

	/** How fast the accumulated kick is pulled back to rest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Return", meta = (ClampMin = 0.0))
	float ReturnSpeed = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Spring", meta = (ClampMin = 0.0))
	float LocationStiffness = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Spring", meta = (ClampMin = 0.0))
	float LocationDamping = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Spring", meta = (ClampMin = 0.001))
	float LocationMass = 0.5f;

	/** How much of the target's own movement the spring carries through; see VectorSpringInterp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Spring", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float LocationTargetVelocity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Spring", meta = (ClampMin = 0.0))
	float RotationStiffness = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Spring", meta = (ClampMin = 0.0))
	float RotationDamping = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Spring", meta = (ClampMin = 0.001))
	float RotationMass = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil|Spring", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float RotationTargetVelocity = 0.0f;
};

/**
 * The viewmodel's procedural recoil: the kick a shot gives the arms and weapon on screen, on top
 * of whatever the fire montage does.
 *
 * This component computes the kick; the viewmodel's animation graph applies it:
 * added to the bone the weapon is held by, after which IK brings both hands back onto the
 * weapon. The weapon and hands kick and the shoulders stay put. Moving the whole viewmodel mesh
 * instead swung the arms from the shoulder with every shot, far more motion on screen for the
 * same values. URSViewmodelAnimInstance reads the offset; the graph only wires it in.
 *
 * Kicked only where the viewmodel is what is seen — the shooter's machine, or a dead teammate's
 * watching them in first person (see ARSPlayerCharacter::IsViewedInFirstPerson); everywhere else it
 * stays at rest. It ticks only while there is a kick to resolve — this, like the spread blend, is an
 * interpolation against real time, the documented exception to "no polling on tick".
 */
UCLASS(ClassGroup = (RS), meta = (BlueprintSpawnableComponent))
class ROUND_SURVIVAL_API URSFirstPersonRecoilComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URSFirstPersonRecoilComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** One shot's kick. The spring and return settings of the weapon that fired it take over. */
	void AddRecoil(const FRSWeaponRecoil& Recoil);

	/** The current kick, to add to the weapon bone: cm along, and degrees about, the mesh's axes. */
	FVector GetLocationOffset() const { return CurrentLocation; }
	FRotator GetRotationOffset() const { return FRotator(CurrentRotation.X, CurrentRotation.Y, CurrentRotation.Z); }

	//~UActorComponent interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UActorComponent interface

protected:
	//~UActorComponent interface
	virtual void BeginPlay() override;
	//~End of UActorComponent interface

private:
	/** The viewmodel mesh. Without one there is nothing to kick. */
	TWeakObjectPtr<USceneComponent> Viewmodel;

	/** The settings of the last weapon that fired: they govern the return and the spring. */
	FRSWeaponRecoil Settings;

	/** Where the kick wants the viewmodel; rotation is pitch, yaw, roll in degrees. */
	FVector TargetLocation = FVector::ZeroVector;
	FVector TargetRotation = FVector::ZeroVector;

	/** Where the spring has it. */
	FVector CurrentLocation = FVector::ZeroVector;
	FVector CurrentRotation = FVector::ZeroVector;

	FVectorSpringState LocationSpring;
	FVectorSpringState RotationSpring;
};
