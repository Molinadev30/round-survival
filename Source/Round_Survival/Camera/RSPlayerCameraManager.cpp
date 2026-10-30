// Round Survival

#include "Camera/RSPlayerCameraManager.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

ARSPlayerCameraManager::ARSPlayerCameraManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ARSPlayerCameraManager::BeginSpectating()
{
	bSpectating = true;
	SpectatorView = DefaultSpectatorView;
}

void ARSPlayerCameraManager::EndSpectating()
{
	bSpectating = false;
}

void ARSPlayerCameraManager::SetSpectatorView(ERSSpectatorView NewView)
{
	SpectatorView = NewView;
}

void ARSPlayerCameraManager::ToggleSpectatorView()
{
	if (!bSpectating)
	{
		return;
	}

	SetSpectatorView(SpectatorView == ERSSpectatorView::FirstPerson ? ERSSpectatorView::ThirdPerson : ERSSpectatorView::FirstPerson);
}

void ARSPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);

	// Checked after the update rather than where the target is set, because there is no one place:
	// the server's choice arrives as a player state that the update itself resolves to a pawn, a
	// client is sent the pawn, a watched teammate respawning swaps the pawn under the same player,
	// and on a client the pawn can arrive before its player state does. The engine settles all of
	// them into ViewTarget's player state during the update; this only notices the result.
	APlayerState* ViewedPlayerState = GetViewedPlayerState();
	if (ViewedPlayerState == ReportedPlayerState.Get() && bSpectating == bReportedSpectating && SpectatorView == ReportedSpectatorView)
	{
		return;
	}

	ReportedPlayerState = ViewedPlayerState;
	bReportedSpectating = bSpectating;
	ReportedSpectatorView = SpectatorView;

	OnViewChanged.Broadcast();
}

void ARSPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	// Through the eyes, the view target's own camera answers, as it always does.
	if (!IsViewingTargetFromOutside() || !OutVT.Target || OutVT.Target == PCOwner)
	{
		Super::UpdateViewTargetInternal(OutVT, DeltaTime);
		return;
	}

	// From outside, an orbit turned by the spectator's own look. The engine's FreeCam style does the
	// same sums, but it is a debug view: it skips camera modifiers and centres on the actor, which for
	// a teammate on another machine is the capsule jumping to each network update.
	const FRotator Rotation = PCOwner->GetControlRotation();
	const FVector Pivot = GetSmoothedLocation(OutVT.Target);
	const FVector Desired = Pivot - Rotation.Vector() * SpectatorCameraDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RSSpectatorOrbit), /*bTraceComplex=*/ false, this);
	Params.AddIgnoredActor(OutVT.Target);

	FHitResult Hit;
	GetWorld()->SweepSingleByChannel(Hit, Pivot, Desired, FQuat::Identity, ECC_Camera,
		FCollisionShape::MakeBox(FVector(SpectatorCameraProbeExtent)), Params);

	OutVT.POV.Location = Hit.bBlockingHit ? Hit.Location : Desired;
	OutVT.POV.Rotation = Rotation;
}

FVector ARSPlayerCameraManager::GetSmoothedLocation(const AActor* Target)
{
	// The engine's network smoothing moves a character's mesh, never its capsule: the mesh sits at
	// its base offset from the capsule plus a correction that eases to nothing. Taking the base
	// offset back off the mesh gives where the capsule would be if it moved as smoothly.
	const ACharacter* Character = Cast<ACharacter>(Target);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return Target->GetActorLocation();
	}

	return Mesh->GetComponentLocation() - Character->GetActorQuat().RotateVector(Character->GetBaseTranslationOffset());
}
