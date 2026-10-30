// Round Survival

#include "Interaction/RSSectorGate.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "RSLogChannels.h"
#include "Rounds/RSZone.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

ARSSectorGate::ARSSectorGate()
{
	// Ticks only while the panel is sinking; see ApplyOpened.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Whether the doorway blocks is collision a client predicts its movement against, so every
	// client needs it wherever it stands. A map has a handful of gates and each changes once.
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	DisplayName = LOCTEXT("SectorGateName", "Door");

	// Ground zero, like the barricade: the gate is placed on the floor of its doorway, and the panel
	// hangs from the origin rather than being it, so it can sink without moving the actor.
	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	SetRootComponent(Origin);

	Mesh->SetupAttachment(Origin);
	Mesh->SetMobility(EComponentMobility::Movable);

	// The panel blocks movement, shots and the interaction trace, and nothing else needs telling:
	// that is its default profile. It is kept out of the navigation, which NavBlocker answers for.
	Mesh->SetCanEverAffectNavigation(false);

	// Greybox fallback, a doorway-sized slab with +X facing out. A Blueprint can swap the mesh and
	// its transform; the navigation follows whatever it is given.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.3f, 2.0f, 3.0f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	}

	NavBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("NavBlocker"));
	NavBlocker->SetupAttachment(Origin);
	NavBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavBlocker->SetGenerateOverlapEvents(false);
	NavBlocker->SetCanEverAffectNavigation(true);
	// Exported as an area modifier rather than as geometry, which is what lets it come out of the
	// navmesh at runtime without a rebuild of the tiles around it. Null, not the system's default
	// obstacle area: that one only makes a path expensive, and an enemy with no other way round
	// would still walk into the panel.
	NavBlocker->bDynamicObstacle = true;
	NavBlocker->SetAreaClassOverride(UNavArea_Null::StaticClass());
}

void ARSSectorGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	FitNavBlockerToPanel();
}

void ARSSectorGate::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// OnConstruction is the editor's preview: a cooked game does not run it for a gate placed in
	// the level, and what it did is not saved with it.
	FitNavBlockerToPanel();
}

void ARSSectorGate::FitNavBlockerToPanel()
{
	const UStaticMesh* PanelMesh = Mesh->GetStaticMesh();
	if (PanelMesh == nullptr)
	{
		return;
	}

	const FBoxSphereBounds LocalBounds = PanelMesh->GetBounds();
	const FVector Scale = Mesh->GetRelativeScale3D();
	const FQuat Rotation = Mesh->GetRelativeRotation().Quaternion();

	NavBlocker->SetRelativeLocationAndRotation(Mesh->GetRelativeLocation() + Rotation.RotateVector(LocalBounds.Origin * Scale), Rotation);
	NavBlocker->SetBoxExtent(LocalBounds.BoxExtent * Scale.GetAbs(), /*bUpdateOverlaps=*/ false);
}

void ARSSectorGate::BeginPlay()
{
	Super::BeginPlay();

	// A client that arrives after the gate was bought: the notify may already have run, before
	// there was anything to animate for.
	if (bOpened)
	{
		ApplyOpened(/*bAnimate=*/ false);
	}
}

bool ARSSectorGate::IsAvailable_Implementation(AActor* Interactor) const
{
	return !bOpened;
}

void ARSSectorGate::OnPurchased_Implementation(AActor* Interactor)
{
	bOpened = true;

	UE_LOG(LogRS, Log, TEXT("[%s] opened [%s]."), *GetNameSafe(Interactor), *GetName());

	// The server gets no rep notify.
	ApplyOpened(/*bAnimate=*/ true);

	for (ARSZone* Zone : ZonesToOpen)
	{
		if (Zone)
		{
			Zone->SetOpen(true);
		}
	}
}

void ARSSectorGate::OnRep_Opened()
{
	if (bOpened)
	{
		ApplyOpened(/*bAnimate=*/ HasActorBegunPlay());
	}
}

void ARSSectorGate::ApplyOpened(bool bAnimate)
{
	// Open on the first frame: nothing waits for the panel to finish moving. With no collision the
	// interaction trace passes through as well, so the prompt goes with it.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavBlocker->SetCanEverAffectNavigation(false);

	if (!bAnimate || OpenSeconds <= 0.0f)
	{
		Mesh->SetVisibility(false);
		SetActorTickEnabled(false);
		return;
	}

	PanelClosedLocation = Mesh->GetRelativeLocation();
	PanelDrop = Mesh->Bounds.BoxExtent.Z * 2.0f;
	OpenElapsed = 0.0f;
	SetActorTickEnabled(true);
}

void ARSSectorGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	OpenElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(OpenElapsed / OpenSeconds, 0.0f, 1.0f);

	Mesh->SetRelativeLocation(PanelClosedLocation - FVector(0.0f, 0.0f, PanelDrop * FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f)));

	if (Alpha >= 1.0f)
	{
		Mesh->SetVisibility(false);
		SetActorTickEnabled(false);
	}
}

void ARSSectorGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bOpened);
}

#undef LOCTEXT_NAMESPACE
