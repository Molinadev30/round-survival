// Round Survival

#include "Enemies/RSSeveredPiece.h"

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemies/RSDismembermentSettings.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"

namespace RSSeveredPiece
{
	/** A named profile (DefaultEngine.ini): physics only, against the world and other bodies. */
	static const FName CollisionProfile(TEXT("RSSeveredPiece"));
}

ARSSeveredPiece::ARSSeveredPiece(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Each machine throws its own; there is nothing to send.
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(RSSeveredPiece::CollisionProfile);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->bReceivesDecals = false;
	RootComponent = Mesh;

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ARSSeveredPiece::Launch(USkeletalMesh* PieceMesh, const USkeletalMeshComponent& Source, const FVector& Velocity,
	UNiagaraSystem* TrailEffect, float Lifetime)
{
	Release();

	// The piece's own physics asset comes with its mesh: each piece's bodies are shaped to
	// the piece, which the whole body's asset does not.
	Mesh->SetSkeletalMesh(PieceMesh);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// Bodies made fresh on every throw. A piece that has never been out has none (the engine makes
	// none for a component that cannot collide), and one coming back has the last throw's, laid
	// out from the pose it ended in.
	Mesh->RecreatePhysicsState();
	FBodyInstance* const* RootBody = Mesh->Bodies.IsValidIndex(Mesh->RootBodyData.BodyIndex) ? &Mesh->Bodies[Mesh->RootBodyData.BodyIndex] : nullptr;
	const FName RootBone = RootBody && *RootBody && (*RootBody)->BodySetup.IsValid() ? (*RootBody)->BodySetup->BoneName : NAME_None;
	const int32 RootBoneIndex = PieceMesh->GetRefSkeleton().FindBoneIndex(RootBone);
	if (RootBoneIndex == INDEX_NONE || Source.GetBoneIndex(RootBone) == INDEX_NONE)
	{
		Release();
		return;
	}

	// The component where the piece's reference pose has the root body's bone on the body it came
	// off. The bones with no body of their own hang from the component in that pose, so they stay
	// with the piece; the body's own component would leave them beside it, wherever the part was
	// in the animation.
	FTransform PieceTransform = FAnimationRuntime::GetComponentSpaceTransformRefPose(PieceMesh->GetRefSkeleton(), RootBoneIndex).Inverse()
		* Source.GetSocketTransform(RootBone, RTS_World);
	PieceTransform.SetScale3D(Source.GetComponentScale());
	SetActorTransform(PieceTransform, /*bSweep=*/ false, nullptr, ETeleportType::ResetPhysics);

	Mesh->SetAllBodiesSimulatePhysics(true);

	// Every body where the same bone was on the body it came off, then all of them moving as one.
	// Simulation starting from the reference pose would put the arm out sideways, beside the stump.
	for (FBodyInstance* Body : Mesh->Bodies)
	{
		if (Body && Body->IsValidBodyInstance() && Body->BodySetup.IsValid())
		{
			const FName Bone = Body->BodySetup->BoneName;
			if (Source.GetBoneIndex(Bone) != INDEX_NONE)
			{
				Body->SetBodyTransform(Source.GetSocketTransform(Bone, RTS_World), ETeleportType::TeleportPhysics);
			}
		}
	}

	// The simulation carries the component at a fixed offset from the root body, taken when the
	// bodies were made, from wherever they were made. Taken again with every body in place.
	Mesh->SetRootBodyIndex(Mesh->RootBodyData.BodyIndex);

	Mesh->SetAllPhysicsLinearVelocity(Velocity);
	Mesh->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Mesh->WakeAllRigidBodies();

	// On the cut end: the root body is the one that came off the stump.
	if (TrailEffect)
	{
		Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(TrailEffect, Mesh, RootBone,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, /*bAutoDestroy=*/ false);
	}

	bLaunched = true;
	LaunchTime = GetWorld()->GetTimeSeconds();
	if (Lifetime > 0.0f)
	{
		GetWorldTimerManager().SetTimer(LifetimeHandle, this, &ThisClass::Release, Lifetime, /*bLoop=*/ false);
	}
}

void ARSSeveredPiece::Release()
{
	GetWorldTimerManager().ClearTimer(LifetimeHandle);

	if (Trail)
	{
		Trail->DestroyComponent();
		Trail = nullptr;
	}

	Mesh->SetAllBodiesSimulatePhysics(false);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	bLaunched = false;
}

URSSeveredPieceSubsystem* URSSeveredPieceSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URSSeveredPieceSubsystem>() : nullptr;
}

bool URSSeveredPieceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

ARSSeveredPiece* URSSeveredPieceSubsystem::Launch(USkeletalMesh* PieceMesh, const USkeletalMeshComponent& Source,
	const FVector& Velocity, UNiagaraSystem* TrailEffect)
{
	const URSDismembermentSettings* Settings = GetDefault<URSDismembermentSettings>();
	UWorld* World = GetWorld();
	if (!PieceMesh || !World || Settings->MaxSeveredPieces <= 0)
	{
		return nullptr;
	}

	// One lying idle, else a new one while there is room, else the oldest one out.
	ARSSeveredPiece* Piece = nullptr;
	for (ARSSeveredPiece* Candidate : Pieces)
	{
		if (Candidate && !Candidate->IsLaunched())
		{
			Piece = Candidate;
			break;
		}
	}

	if (!Piece && Pieces.Num() < Settings->MaxSeveredPieces)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		Piece = World->SpawnActor<ARSSeveredPiece>(SpawnParameters);
		if (Piece)
		{
			Pieces.Add(Piece);
		}
	}

	if (!Piece)
	{
		for (ARSSeveredPiece* Candidate : Pieces)
		{
			if (Candidate && (!Piece || Candidate->GetLaunchTime() < Piece->GetLaunchTime()))
			{
				Piece = Candidate;
			}
		}
	}

	if (Piece)
	{
		Piece->Launch(PieceMesh, Source, Velocity, TrailEffect, Settings->SeveredPieceLifetime);
	}
	return Piece;
}
