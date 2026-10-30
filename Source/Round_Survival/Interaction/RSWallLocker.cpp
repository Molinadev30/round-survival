// Round Survival

#include "Interaction/RSWallLocker.h"

#include "Components/StaticMeshComponent.h"
#include "Equipment/RSEquipmentDefinition.h"
#include "RSCollisionChannels.h"
#include "RSLogChannels.h"
#include "Weapons/RSWeaponAssemblyComponent.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponItemInstance.h"
#include "Weapons/RSWeaponSlotsComponent.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

ARSWallLocker::ARSWallLocker()
{
	// The name and both prices come from the weapon.
	bHasFixedNameAndCost = false;

	DisplayAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("DisplayAnchor"));
	DisplayAnchor->SetupAttachment(Mesh);

	DisplayMesh = CreateDefaultSubobject<URSWeaponAssemblyComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(DisplayAnchor);
	DisplayMesh->bCentreOnAttachParent = true;

	// The panel is scaled to shape it; the weapon keeps its real size whatever the panel does.
	DisplayMesh->SetUsingAbsoluteScale(true);

	// A prop in its reference pose: nothing animates it, so it never needs to tick or refresh bones.
	// Its parts follow the receiver in this, so the magazine on the wall is just as still.
	DisplayMesh->PrimaryComponentTick.bCanEverTick = false;
	DisplayMesh->bNoSkeletonUpdate = true;

	// Only the interaction trace hits it, so aiming at the weapon itself focuses the locker. The
	// panel behind it is still the reliable target: a mesh with no physics asset has no collision.
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DisplayMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	DisplayMesh->SetCollisionResponseToChannel(RS_TraceChannel_Interaction, ECR_Block);
	DisplayMesh->SetGenerateOverlapEvents(false);
	DisplayMesh->CanCharacterStepUpOn = ECB_No;
}

void ARSWallLocker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RefreshDisplay();
}

void ARSWallLocker::BeginPlay()
{
	Super::BeginPlay();

	RefreshDisplay();
}

void ARSWallLocker::RefreshDisplay()
{
	DisplayMesh->SetAssembly(WeaponItem ? WeaponItem->Assembly.Get() : nullptr);

	// The finish the weapon comes with, so what hangs on the wall is what the player walks away with.
	DisplayMesh->ApplySkin(WeaponItem ? WeaponItem->Skin.Get() : nullptr);
}

URSWeaponSlotsComponent* ARSWallLocker::FindWeaponSlots(AActor* Interactor)
{
	return Interactor ? Interactor->FindComponentByClass<URSWeaponSlotsComponent>() : nullptr;
}

URSWeaponItemInstance* ARSWallLocker::FindCarriedCopy(AActor* Interactor) const
{
	const URSWeaponSlotsComponent* Slots = FindWeaponSlots(Interactor);
	return Slots ? Slots->FindItemByDefinition(WeaponItem) : nullptr;
}

FText ARSWallLocker::GetWeaponName() const
{
	if (WeaponItem == nullptr)
	{
		return FText::FromString(GetName());
	}

	return WeaponItem->DisplayName.IsEmpty() ? FText::FromString(WeaponItem->GetName()) : WeaponItem->DisplayName;
}

int32 ARSWallLocker::GetScrapCost_Implementation(AActor* Interactor) const
{
	if (WeaponItem == nullptr)
	{
		return 0;
	}

	// The carried copy names its refill price: an upgraded one costs more to feed.
	const URSWeaponItemInstance* CarriedCopy = FindCarriedCopy(Interactor);
	return CarriedCopy ? CarriedCopy->GetRefillCost() : WeaponItem->PurchaseCost;
}

bool ARSWallLocker::IsAvailable_Implementation(AActor* Interactor) const
{
	// A locker that names no weapon, or a weapon that cannot be drawn, would take the scrap and hand
	// over nothing.
	if (WeaponItem == nullptr || WeaponItem->EquipmentDefinition == nullptr)
	{
		return false;
	}

	// Something with no slots cannot carry what this sells.
	if (FindWeaponSlots(Interactor) == nullptr)
	{
		return false;
	}

	// Refused before paying: a refill of a full weapon is a charge for nothing.
	const URSWeaponItemInstance* CarriedCopy = FindCarriedCopy(Interactor);
	return CarriedCopy == nullptr || !CarriedCopy->IsReserveAmmoFull();
}

FText ARSWallLocker::GetPromptName(AActor* Interactor) const
{
	// The ammo is for the copy the player carries, so it goes by that copy's name, upgraded or not.
	const URSWeaponItemInstance* CarriedCopy = FindCarriedCopy(Interactor);
	return CarriedCopy
		? FText::Format(LOCTEXT("WallLockerRefill", "{0} ammo"), CarriedCopy->GetDisplayName())
		: GetWeaponName();
}

FText ARSWallLocker::GetUnavailablePrompt(AActor* Interactor) const
{
	if (const URSWeaponItemInstance* CarriedCopy = FindCarriedCopy(Interactor))
	{
		return FText::Format(LOCTEXT("WallLockerFull", "{0} ammo is full"), CarriedCopy->GetDisplayName());
	}

	return GetWeaponName();
}

void ARSWallLocker::OnPurchased_Implementation(AActor* Interactor)
{
	URSWeaponSlotsComponent* Slots = FindWeaponSlots(Interactor);
	if (Slots == nullptr || WeaponItem == nullptr)
	{
		// IsAvailable refuses both before the cost is committed; reaching here means it was bypassed.
		UE_LOG(LogRS, Error, TEXT("[%s] was paid by [%s] but has nothing to give."), *GetName(), *GetNameSafe(Interactor));
		return;
	}

	// Asked again rather than remembered from the prompt: this is the server's answer, at the moment
	// the scrap was taken, and the price charged was decided by the same question.
	if (URSWeaponItemInstance* CarriedCopy = Slots->FindItemByDefinition(WeaponItem))
	{
		CarriedCopy->RefillReserveAmmo();
		UE_LOG(LogRS, Log, TEXT("[%s] refilled [%s] at [%s]."), *GetNameSafe(Interactor), *GetNameSafe(WeaponItem), *GetName());
	}
	else if (Slots->GiveWeapon(WeaponItem, /*bDraw=*/ true))
	{
		UE_LOG(LogRS, Log, TEXT("[%s] bought [%s] at [%s]."), *GetNameSafe(Interactor), *GetNameSafe(WeaponItem), *GetName());
	}
	else
	{
		UE_LOG(LogRS, Error, TEXT("[%s] paid for [%s] at [%s] but GiveWeapon refused it."), *GetNameSafe(Interactor), *GetNameSafe(WeaponItem), *GetName());
	}
}

#undef LOCTEXT_NAMESPACE
