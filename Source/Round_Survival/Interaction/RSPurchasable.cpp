// Round Survival

#include "Interaction/RSPurchasable.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/RSEconomySet.h"
#include "Components/StaticMeshComponent.h"
#include "Core/RSGameState.h"
#include "Engine/World.h"
#include "RSLogChannels.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

ARSPurchasable::ARSPurchasable()
{
	PrimaryActorTick.bCanEverTick = false;

	// Purchasables replicate: whether a gate is open or a locker has been bought is world state
	// every client has to agree on.
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

int32 ARSPurchasable::GetScrapCost_Implementation(AActor* Interactor) const
{
	return Cost;
}

float ARSPurchasable::GetHoldSeconds_Implementation(AActor* Interactor) const
{
	return HoldSeconds;
}

bool ARSPurchasable::IsRepeatable_Implementation() const
{
	return bRepeatable;
}

bool ARSPurchasable::CanAfford(AActor* Interactor) const
{
	const int32 Price = IRSInteractable::Execute_GetScrapCost(this, Interactor);
	if (Price <= 0)
	{
		return true;
	}

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Interactor);
	if (ASC == nullptr)
	{
		return false;
	}

	bool bFound = false;
	const float Scrap = ASC->GetGameplayAttributeValue(URSEconomySet::GetScrapAttribute(), bFound);

	return bFound && Scrap >= static_cast<float>(Price);
}

bool ARSPurchasable::HasPower() const
{
	if (!bCanRequirePower || !bRequiresPower)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const ARSGameState* GameState = World ? World->GetGameState<ARSGameState>() : nullptr;

	return GameState && GameState->IsPowerOn();
}

bool ARSPurchasable::CanInteract_Implementation(AActor* Interactor) const
{
	return HasPower() && IsAvailable(Interactor) && CanAfford(Interactor);
}

FText ARSPurchasable::GetPromptName(AActor* Interactor) const
{
	return DisplayName.IsEmpty() ? FText::FromString(GetName()) : DisplayName;
}

FText ARSPurchasable::GetUnavailablePrompt(AActor* Interactor) const
{
	return GetPromptName(Interactor);
}

FText ARSPurchasable::GetPrompt_Implementation(AActor* Interactor) const
{
	// No key and no price: there is nothing to press until somebody finds the Power Core.
	if (!HasPower())
	{
		return LOCTEXT("PurchasableNoPower", "Requires Power");
	}

	if (!IsAvailable(Interactor))
	{
		return GetUnavailablePrompt(Interactor);
	}

	const FText Name = GetPromptName(Interactor);
	const int32 Price = IRSInteractable::Execute_GetScrapCost(this, Interactor);

	// The same sentence whether or not the player can afford it: the price is the message, and a
	// player short of scrap reads it the same way as one who is not.
	const FText Action = Price > 0
		? FText::Format(LOCTEXT("PurchasableAction", "for {0} [Cost: {1}]"), Name, FText::AsNumber(Price))
		: FText::Format(LOCTEXT("PurchasableActionFree", "for {0}"), Name);

	return IRSInteractable::MakeActionPrompt(Action, IRSInteractable::Execute_GetHoldSeconds(this, Interactor));
}

void ARSPurchasable::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRS, Error, TEXT("ARSPurchasable::Interact ran without authority on [%s]."), *GetName());
		return;
	}

	UE_LOG(LogRS, Verbose, TEXT("[%s] purchased [%s] for %d scrap."),
		*GetNameSafe(Interactor), *GetName(), IRSInteractable::Execute_GetScrapCost(this, Interactor));

	// The scrap is already gone: the ability committed its cost before calling this.
	OnPurchased(Interactor);
}

#undef LOCTEXT_NAMESPACE
