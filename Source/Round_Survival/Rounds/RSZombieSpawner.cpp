// Round Survival

#include "Rounds/RSZombieSpawner.h"

#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "RSLogChannels.h"
#include "Rounds/RSSpawnSubsystem.h"
#include "Rounds/RSZone.h"

#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#endif

ARSZombieSpawner::ARSZombieSpawner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Server only and invisible. Nothing about a spawn point is a client's business.
	bReplicates = false;
	SetHidden(true);

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (ArrowComponent)
	{
		ArrowComponent->SetupAttachment(RootComponent);
		ArrowComponent->ArrowSize = 1.5f;
		ArrowComponent->SetArrowColor(FLinearColor(1.0f, 0.35f, 0.1f));
		ArrowComponent->bIsScreenSizeScaled = true;
	}

	BillboardComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (BillboardComponent)
	{
		BillboardComponent->SetupAttachment(RootComponent);
		BillboardComponent->bIsScreenSizeScaled = true;
	}
#endif
}

void ARSZombieSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Spawn points are a server-side concern; on a client this actor should not exist at all.
	if (!HasAuthority())
	{
		return;
	}

	if (!Zone)
	{
		UE_LOG(LogRSRounds, Verbose, TEXT("Spawner [%s] has no zone and is always available."), *GetNameSafe(this));
	}

	if (URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this))
	{
		SpawnSubsystem->RegisterSpawner(this);
	}
}

void ARSZombieSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this))
	{
		SpawnSubsystem->UnregisterSpawner(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool ARSZombieSpawner::IsZoneOpen() const
{
	// A point with no zone belongs to the map itself: the starting area, or a test level.
	return Zone == nullptr || Zone->IsOpen();
}

float ARSZombieSpawner::GetTimeSinceLastSpawn() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return TNumericLimits<float>::Max();
	}

	return static_cast<float>(World->GetTimeSeconds() - LastSpawnTime);
}

bool ARSZombieSpawner::IsOnCooldown() const
{
	return SpawnCooldown > 0.0f && GetTimeSinceLastSpawn() < SpawnCooldown;
}

bool ARSZombieSpawner::IsBlocked() const
{
	const UWorld* World = GetWorld();
	if (!World || ClearanceRadius <= 0.0f)
	{
		return false;
	}

	// Lifted by the radius so the sphere sits above the floor rather than half inside it.
	const FVector TestLocation = GetGroundSpawnTransform().GetLocation() + FVector(0.0f, 0.0f, ClearanceRadius);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RSSpawnerClearance), /*bTraceComplex=*/ false);
	QueryParams.AddIgnoredActor(this);

	return World->OverlapAnyTestByChannel(TestLocation, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(ClearanceRadius), QueryParams);
}

bool ARSZombieSpawner::IsEligible() const
{
	return IsZoneOpen() && !IsOnCooldown() && !IsBlocked();
}

FTransform ARSZombieSpawner::GetGroundSpawnTransform() const
{
	FTransform SpawnTransform(GetActorRotation(), GetActorLocation());

	if (!bProjectToNavigation)
	{
		return SpawnTransform;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		return SpawnTransform;
	}

	// A point that has drifted off the floor after a level edit still produces an enemy that can
	// walk, instead of one that spawns in the air and falls through the geometry.
	FNavLocation ProjectedLocation;
	if (NavSystem->ProjectPointToNavigation(SpawnTransform.GetLocation(), ProjectedLocation,
		FVector(NavigationProjectionExtent)))
	{
		SpawnTransform.SetLocation(ProjectedLocation.Location);
	}

	return SpawnTransform;
}

void ARSZombieSpawner::NotifyEnemySpawned(ARSZombieCharacter* Enemy)
{
	if (const UWorld* World = GetWorld())
	{
		LastSpawnTime = World->GetTimeSeconds();
	}

	// Always assigned, including to null. A pooled enemy handed back out from a point with no
	// window would otherwise still be carrying the last one's, and would walk to a barricade on
	// the far side of the map to climb through a window it never came out of.
	if (IsValid(Enemy))
	{
		Enemy->SetEntryBarricade(Barricade);
	}
}
