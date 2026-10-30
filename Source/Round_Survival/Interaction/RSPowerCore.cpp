// Round Survival

#include "Interaction/RSPowerCore.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/RSGameState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RSLogChannels.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

ARSPowerCore::ARSPowerCore()
{
	DisplayName = LOCTEXT("PowerCoreName", "Power");
	Cost = 0;

	// What turns the power on cannot wait for it.
	bCanRequirePower = false;

	// Ground zero, like the Sector Gate: the core is placed on the floor and its parts stand on it.
	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	SetRootComponent(Origin);

	Mesh->SetupAttachment(Origin);

	// Greybox fallback, a cabinet with +X facing out. A Blueprint swaps the mesh and the light's place.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.5f, 1.0f, 1.8f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	}

	PowerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PowerLight"));
	PowerLight->SetupAttachment(Origin);
	PowerLight->SetMobility(EComponentMobility::Movable);
	PowerLight->SetRelativeLocation(FVector(45.0f, 0.0f, 150.0f));
	PowerLight->SetIntensityUnits(ELightUnits::Candelas);
	PowerLight->SetIntensity(50.0f);
	PowerLight->SetAttenuationRadius(600.0f);
	PowerLight->SetLightColor(FLinearColor(0.35f, 0.8f, 1.0f));
	PowerLight->SetCastShadows(false);
	PowerLight->SetVisibility(false);
}

void ARSPowerCore::BeginPlay()
{
	Super::BeginPlay();

	// On a client the game state is already there: level actors begin play once it has replicated.
	if (ARSGameState* GameState = GetWorld()->GetGameState<ARSGameState>())
	{
		PowerChangedHandle = GameState->OnPowerChanged.AddUObject(this, &ThisClass::HandlePowerChanged);

		// A client that arrives with the power already on has missed the broadcast.
		HandlePowerChanged(GameState->IsPowerOn());
	}
}

void ARSPowerCore::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ARSGameState* GameState = GetWorld()->GetGameState<ARSGameState>())
	{
		GameState->OnPowerChanged.Remove(PowerChangedHandle);
	}

	PowerChangedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

bool ARSPowerCore::IsPowerOn() const
{
	const ARSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARSGameState>() : nullptr;
	return GameState && GameState->IsPowerOn();
}

bool ARSPowerCore::IsAvailable_Implementation(AActor* Interactor) const
{
	return !IsPowerOn();
}

FText ARSPowerCore::GetPrompt_Implementation(AActor* Interactor) const
{
	// Once the power is on there is nothing left to say: no prompt, as with an open gate.
	if (!IsAvailable(Interactor))
	{
		return FText::GetEmpty();
	}

	const int32 Price = IRSInteractable::Execute_GetScrapCost(this, Interactor);
	const FText Action = Price > 0
		? FText::Format(LOCTEXT("PowerCoreActionCost", "to turn on {0} [Cost: {1}]"), GetPromptName(Interactor), FText::AsNumber(Price))
		: FText::Format(LOCTEXT("PowerCoreAction", "to turn on {0}"), GetPromptName(Interactor));

	return IRSInteractable::MakeActionPrompt(Action, IRSInteractable::Execute_GetHoldSeconds(this, Interactor));
}

void ARSPowerCore::OnPurchased_Implementation(AActor* Interactor)
{
	ARSGameState* GameState = GetWorld()->GetGameState<ARSGameState>();
	if (GameState == nullptr)
	{
		UE_LOG(LogRS, Error, TEXT("[%s] was used by [%s] with no game state to turn the power on in."), *GetName(), *GetNameSafe(Interactor));
		return;
	}

	UE_LOG(LogRS, Log, TEXT("[%s] turned the power on at [%s]."), *GetNameSafe(Interactor), *GetName());

	// The server's own light comes on through the same delegate as everyone else's.
	GameState->SetPowerOn(true);
}

void ARSPowerCore::HandlePowerChanged(bool bPowerOn)
{
	PowerLight->SetVisibility(bPowerOn);
}

#undef LOCTEXT_NAMESPACE
