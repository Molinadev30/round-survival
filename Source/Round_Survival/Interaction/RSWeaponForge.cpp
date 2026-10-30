// Round Survival

#include "Interaction/RSWeaponForge.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RSLogChannels.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponItemInstance.h"
#include "Weapons/RSWeaponSlotsComponent.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

ARSWeaponForge::ARSWeaponForge()
{
	DisplayName = LOCTEXT("WeaponForgeName", "Weapon Forge");
	Cost = 5000;

	// The wait is part of the price: three seconds with both hands on the forge, in the middle of a round.
	HoldSeconds = 3.0f;

	bRequiresPower = true;

	// Ground zero, like the stations: placed on the floor, with its parts standing on it.
	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	SetRootComponent(Origin);

	Mesh->SetupAttachment(Origin);

	// Greybox fallback, a bench 160 x 90 x 100 cm with +X facing out. A Blueprint swaps the mesh.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.9f, 1.6f, 1.0f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	}
}

URSWeaponItemInstance* ARSWeaponForge::FindWeaponInHand(AActor* Interactor)
{
	const URSWeaponSlotsComponent* Slots = Interactor ? Interactor->FindComponentByClass<URSWeaponSlotsComponent>() : nullptr;
	return Slots ? Slots->GetActiveItem() : nullptr;
}

bool ARSWeaponForge::IsAvailable_Implementation(AActor* Interactor) const
{
	const URSWeaponItemInstance* Weapon = FindWeaponInHand(Interactor);
	return Weapon && Weapon->CanBeUpgraded();
}

FText ARSWeaponForge::GetPromptName(AActor* Interactor) const
{
	// Named after what the player gets: "Hold E for Carbine Mk II [Cost: 5,000]".
	const URSWeaponItemInstance* Weapon = FindWeaponInHand(Interactor);
	const URSWeaponItemDefinition* Definition = Weapon ? Weapon->GetDefinition() : nullptr;

	if (Definition && !Definition->Upgrade.DisplayName.IsEmpty())
	{
		return Definition->Upgrade.DisplayName;
	}

	return Super::GetPromptName(Interactor);
}

FText ARSWeaponForge::GetUnavailablePrompt(AActor* Interactor) const
{
	const URSWeaponItemInstance* Weapon = FindWeaponInHand(Interactor);
	if (Weapon == nullptr)
	{
		return Super::GetUnavailablePrompt(Interactor);
	}

	if (Weapon->GetUpgradeLevel() > 0)
	{
		return FText::Format(LOCTEXT("WeaponForgeAlreadyUpgraded", "{0} is already upgraded"), Weapon->GetDisplayName());
	}

	return FText::Format(LOCTEXT("WeaponForgeCannotUpgrade", "{0} cannot be upgraded"), Weapon->GetDisplayName());
}

void ARSWeaponForge::OnPurchased_Implementation(AActor* Interactor)
{
	// Asked again rather than remembered from the prompt. Switching weapons is refused for the whole
	// hold, so this is the weapon the hold started on.
	URSWeaponItemInstance* Weapon = FindWeaponInHand(Interactor);

	if (Weapon && Weapon->ApplyUpgrade())
	{
		UE_LOG(LogRS, Log, TEXT("[%s] upgraded [%s] at [%s]."), *GetNameSafe(Interactor), *GetNameSafe(Weapon->GetDefinition()), *GetName());
	}
	else
	{
		UE_LOG(LogRS, Error, TEXT("[%s] paid at [%s] but [%s] could not be upgraded."), *GetNameSafe(Interactor), *GetName(),
			*GetNameSafe(Weapon ? Weapon->GetDefinition() : nullptr));
	}
}

#undef LOCTEXT_NAMESPACE
