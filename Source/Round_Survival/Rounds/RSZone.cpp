// Round Survival

#include "Rounds/RSZone.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "RSLogChannels.h"
#include "Rounds/RSSpawnSubsystem.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#endif

ARSZone::ARSZone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	bReplicates = true;
	bAlwaysRelevant = true;
	SetHidden(true);

#if WITH_EDITORONLY_DATA
	// The trigger sprite rather than the default one, so a zone does not read as one of its spawners.
	BillboardComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (BillboardComponent)
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> ZoneSprite(TEXT("/Engine/EditorResources/S_Trigger"));

		BillboardComponent->SetupAttachment(RootComponent);
		BillboardComponent->SetSprite(ZoneSprite.Get());
		BillboardComponent->bIsScreenSizeScaled = true;
	}
#endif
}

void ARSZone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		bIsOpen = bStartsOpen;
	}

	if (URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this))
	{
		SpawnSubsystem->RegisterZone(this);
	}
}

void ARSZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this))
	{
		SpawnSubsystem->UnregisterZone(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ARSZone::SetOpen(bool bNewOpen)
{
	if (!HasAuthority() || bIsOpen == bNewOpen)
	{
		return;
	}

	bIsOpen = bNewOpen;

	UE_LOG(LogRSRounds, Log, TEXT("Zone [%s] is now %s."), *GetNameSafe(this), bIsOpen ? TEXT("open") : TEXT("closed"));

	// The server does not get an OnRep. Broadcasting here is what keeps a listen server host's own
	// HUD and gate in step with the clients'.
	OnZoneOpenChanged.Broadcast(this, bIsOpen);
}

void ARSZone::OnRep_IsOpen()
{
	OnZoneOpenChanged.Broadcast(this, bIsOpen);
}

void ARSZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bIsOpen);
}
