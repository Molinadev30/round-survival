// Round Survival

#include "Weapons/RSWeaponActor.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Weapons/RSWeaponAssemblyComponent.h"

ARSWeaponActor::ARSWeaponActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Cosmetic only. The equipment instance that spawns this is the replicated half.
	bReplicates = false;

	Assembly = CreateDefaultSubobject<URSWeaponAssemblyComponent>(TEXT("ReceiverMesh"));
	SetRootComponent(Assembly);
}

UNiagaraComponent* ARSWeaponActor::SpawnMuzzleEffect(UNiagaraSystem* System, bool bFollowMuzzle) const
{
	FName Socket;
	FVector Offset = FVector::ZeroVector;
	USceneComponent* Muzzle = System ? Assembly->FindMuzzle(Socket, Offset) : nullptr;
	if (!Muzzle)
	{
		return nullptr;
	}

	// On the world copy, the muzzle itself. On the viewmodel, where the muzzle appears, off the camera.
	USceneComponent* Parent = Muzzle;
	FName ParentSocket = Socket;
	FTransform Relative(Offset);

	const UPrimitiveComponent* MuzzlePrimitive = Cast<UPrimitiveComponent>(Muzzle);
	if (MuzzlePrimitive && MuzzlePrimitive->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson)
	{
		UCameraComponent* Camera = nullptr;
		if (!GetFirstPersonMuzzlePlacement(FTransform(Offset) * Muzzle->GetSocketTransform(Socket), Camera, Relative))
		{
			return nullptr;
		}
		Parent = Camera;
		ParentSocket = NAME_None;
	}

	// Pooled: an automatic weapon spawns these every few frames.
	UNiagaraComponent* Effect = nullptr;
	if (bFollowMuzzle)
	{
		Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Parent, ParentSocket, Relative.GetLocation(), Relative.Rotator(),
			Relative.GetScale3D(), EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/ true, ENCPoolMethod::AutoRelease);
	}
	else
	{
		const FTransform Placement = Relative * Parent->GetSocketTransform(ParentSocket);
		Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Placement.GetLocation(), Placement.Rotator(),
			Placement.GetScale3D(), /*bAutoDestroy=*/ true, /*bAutoActivate=*/ true, ENCPoolMethod::AutoRelease);
	}

	ResetMuzzleEffectVisibility(Effect);
	return Effect;
}

bool ARSWeaponActor::GetFirstPersonMuzzlePlacement(const FTransform& MuzzleTransform, UCameraComponent*& OutCamera, FTransform& OutRelative) const
{
	const AActor* OwningPawn = GetOwner();
	UCameraComponent* Camera = OwningPawn ? OwningPawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Camera)
	{
		return false;
	}

	// The renderer's first-person transform, from FSceneView: in view space the two lateral axes
	// are scaled by tan(FOV / 2) / tan(FirstPersonFOV / 2), and every axis by FirstPersonScale.
	// Camera space here is X forward, Y right, Z up, so X is depth and Y and Z are lateral.
	const float FOVCorrection = Camera->bEnableFirstPersonFieldOfView
		? FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f)) / FMath::Tan(FMath::DegreesToRadians(Camera->FirstPersonFieldOfView * 0.5f))
		: 1.0f;
	const float Scale = Camera->bEnableFirstPersonScale ? Camera->FirstPersonScale : 1.0f;
	const FVector AxisScale(Scale, FOVCorrection * Scale, FOVCorrection * Scale);

	const FTransform& CameraTransform = Camera->GetComponentTransform();
	const FVector ApparentLocation = CameraTransform.InverseTransformPositionNoScale(MuzzleTransform.GetLocation()) * AxisScale;

	// The same scale skews directions, so the barrel's axes go through it too.
	const FVector MuzzleForward = CameraTransform.InverseTransformVectorNoScale(MuzzleTransform.GetUnitAxis(EAxis::X)) * AxisScale;
	const FVector MuzzleUp = CameraTransform.InverseTransformVectorNoScale(MuzzleTransform.GetUnitAxis(EAxis::Z)) * AxisScale;
	const FRotator ApparentRotation = FRotationMatrix::MakeFromXZ(MuzzleForward, MuzzleUp).Rotator();

	// Shrunk with the viewmodel, so the flash keeps its size relative to the weapon on screen.
	OutCamera = Camera;
	OutRelative = FTransform(ApparentRotation, ApparentLocation, FVector(FOVCorrection * Scale));
	return true;
}

void ARSWeaponActor::ResetMuzzleEffectVisibility(UNiagaraComponent* Effect)
{
	if (!Effect)
	{
		return;
	}

	// Visible to whoever renders it, with no owner-only flags. That is already right, because the
	// caller only spawns a flash on one copy per machine, the one that machine's view is drawing
	// (see URSWeaponInstance::PlayFireEffects). Owner flags would not work anyway -- a pooled
	// component has no owner, so OnlyOwnerSee would hide it from everyone. They are set explicitly
	// because the pool hands back components that something else configured.
	Effect->SetOnlyOwnerSee(false);
	Effect->SetOwnerNoSee(false);
	Effect->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
}

UAudioComponent* ARSWeaponActor::PlayMuzzleSound(USoundBase* Sound) const
{
	FName Socket;
	FVector Offset = FVector::ZeroVector;
	USceneComponent* Muzzle = Sound ? Assembly->FindMuzzle(Socket, Offset) : nullptr;

	return Muzzle ? UGameplayStatics::SpawnSoundAttached(Sound, Muzzle, Socket, Offset, EAttachLocation::KeepRelativeOffset,
		/*bStopWhenAttachedToDestroyed=*/ false) : nullptr;
}
