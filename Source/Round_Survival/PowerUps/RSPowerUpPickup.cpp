// Round Survival

#include "PowerUps/RSPowerUpPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/RSGameState.h"
#include "Core/RSPlayerCharacter.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "PowerUps/RSPowerUpDefinition.h"
#include "PowerUps/RSPowerUpEffect.h"
#include "RSLogChannels.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ARSPowerUpPickup::ARSPowerUpPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(false);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(60.0f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);

	// Floats at about waist height above the point it was dropped on.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	Mesh->SetRelativeScale3D(FVector(0.4f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->CastShadow = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicShapeMaterial.Succeeded())
	{
		TintMaterial = BasicShapeMaterial.Object;
	}

	Spin = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spin"));
	Spin->RotationRate = FRotator(0.0f, 90.0f, 0.0f);
	Spin->SetUpdatedComponent(Mesh);
}

void ARSPowerUpPickup::Initialise(const URSPowerUpDefinition* InPowerUp, float InLifetime, float InBlinkSeconds)
{
	PowerUp = InPowerUp;
	Lifetime = InLifetime;
	BlinkSeconds = FMath::Clamp(InBlinkSeconds, 0.0f, InLifetime);
}

void ARSPowerUpPickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleTriggerOverlap);

		if (Lifetime > 0.0f)
		{
			const AGameStateBase* GameState = GetWorld()->GetGameState();
			ExpireServerTime = (GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + Lifetime;

			// Gone without anyone taking it. Nobody else is told: there is nothing to undo.
			GetWorldTimerManager().SetTimer(ExpireTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { Destroy(); }), Lifetime, false);
		}
	}

	RefreshPresentation();

	// A player already standing on the spot picks it up as it lands, rather than having to step off
	// and back on.
	if (HasAuthority())
	{
		TArray<AActor*> Overlapping;
		Trigger->GetOverlappingActors(Overlapping, APawn::StaticClass());
		for (AActor* Actor : Overlapping)
		{
			HandleTriggerOverlap(Trigger, Actor, nullptr, 0, false, FHitResult());
		}
	}
}

void ARSPowerUpPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void ARSPowerUpPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, PowerUp, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ThisClass, ExpireServerTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ThisClass, BlinkSeconds, COND_InitialOnly);
}

void ARSPowerUpPickup::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Players only, and standing ones: a downed player crawling over it does not pick it up.
	const ARSPlayerCharacter* Player = Cast<ARSPlayerCharacter>(OtherActor);
	if (bCollected || !HasAuthority() || !Player || Player->IsDowned() || !PowerUp)
	{
		return;
	}

	bCollected = true;

	FRSPowerUpContext Context;
	Context.World = GetWorld();
	Context.Collector = OtherActor;
	Context.PowerUp = PowerUp;

	UE_LOG(LogRS, Log, TEXT("[%s] picked up [%s]."), *GetNameSafe(OtherActor), *GetNameSafe(PowerUp));

	PowerUp->Activate(Context);
	OnCollected.Broadcast(PowerUp, OtherActor);

	// Told through the game state, not this actor: it is destroyed below, in the same frame.
	if (ARSGameState* GameState = GetWorld()->GetGameState<ARSGameState>())
	{
		GameState->MulticastPowerUpCollected(PowerUp);
	}

	Destroy();
}

void ARSPowerUpPickup::OnRep_PowerUp()
{
	RefreshPresentation();
}

void ARSPowerUpPickup::RefreshPresentation()
{
	if (GetNetMode() == NM_DedicatedServer || !PowerUp)
	{
		return;
	}

	if (PowerUp->Mesh)
	{
		Mesh->SetStaticMesh(PowerUp->Mesh);
	}

	if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(0, TintMaterial))
	{
		Material->SetVectorParameterValue(TEXT("Color"), PowerUp->Color);
	}

	if (BlinkSeconds > 0.0f && ExpireServerTime > 0.0)
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		const float UntilBlink = static_cast<float>(ExpireServerTime - BlinkSeconds - Now);

		if (UntilBlink > 0.0f)
		{
			GetWorldTimerManager().SetTimer(BlinkTimerHandle, this, &ThisClass::StartBlinking, UntilBlink, false);
		}
		else
		{
			StartBlinking();
		}
	}
}

void ARSPowerUpPickup::StartBlinking()
{
	GetWorldTimerManager().SetTimer(BlinkTimerHandle, this, &ThisClass::ToggleBlink, BlinkInterval, /*bLoop=*/ true);
}

void ARSPowerUpPickup::ToggleBlink()
{
	Mesh->SetVisibility(!Mesh->IsVisible());
}
