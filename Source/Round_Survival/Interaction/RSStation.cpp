// Round Survival

#include "Interaction/RSStation.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/RSStationDefinition.h"
#include "RSLogChannels.h"
#include "UObject/ConstructorHelpers.h"

ARSStation::ARSStation()
{
	// The name and the price come from the station definition.
	bHasFixedNameAndCost = false;

	bRequiresPower = true;

	// Ground zero, like the Power Core: placed on the floor, with its parts standing on it.
	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	SetRootComponent(Origin);

	Mesh->SetupAttachment(Origin);

	// Greybox fallback, a machine with +X facing out. A Blueprint swaps the mesh.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.8f, 1.0f, 2.0f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	}
}

int32 ARSStation::GetScrapCost_Implementation(AActor* Interactor) const
{
	return Station ? Station->Cost : 0;
}

FText ARSStation::GetPromptName(AActor* Interactor) const
{
	if (Station == nullptr || Station->DisplayName.IsEmpty())
	{
		return Super::GetPromptName(Interactor);
	}

	return Station->DisplayName;
}

bool ARSStation::IsAvailable_Implementation(AActor* Interactor) const
{
	// A station that names nothing, or names something that cannot be granted, would take the scrap
	// and hand over nothing.
	if (Station == nullptr || !Station->IsValidStation())
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Interactor);
	return ASC && !Station->IsGrantedTo(ASC);
}

void ARSStation::OnPurchased_Implementation(AActor* Interactor)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Interactor);
	if (ASC == nullptr || Station == nullptr)
	{
		// IsAvailable refuses both before the cost is committed; reaching here means it was bypassed.
		UE_LOG(LogRS, Error, TEXT("[%s] was paid by [%s] but has nothing to give."), *GetName(), *GetNameSafe(Interactor));
		return;
	}

	if (Station->GrantTo(ASC).IsValid())
	{
		UE_LOG(LogRS, Log, TEXT("[%s] bought [%s] at [%s]."), *GetNameSafe(Interactor), *GetNameSafe(Station), *GetName());
	}
	else
	{
		UE_LOG(LogRS, Error, TEXT("[%s] paid for [%s] at [%s] but it was not granted."), *GetNameSafe(Interactor), *GetNameSafe(Station), *GetName());
	}
}
