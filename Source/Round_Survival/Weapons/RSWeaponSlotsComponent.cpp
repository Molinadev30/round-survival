// Round Survival

#include "Weapons/RSWeaponSlotsComponent.h"

#include "Engine/ActorChannel.h"
#include "Equipment/RSEquipmentDefinition.h"
#include "Equipment/RSEquipmentInstance.h"
#include "Equipment/RSEquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RSLogChannels.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponItemInstance.h"

URSWeaponSlotsComponent::URSWeaponSlotsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
}

void URSWeaponSlotsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Slots);
	DOREPLIFETIME(ThisClass, ActiveSlotIndex);
}

void URSWeaponSlotsComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Sized on the server only; a client gets the array, empty entries and all, by replication.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Slots.SetNum(NumSlots);
	}
}

URSEquipmentManagerComponent* URSWeaponSlotsComponent::GetEquipmentManager() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<URSEquipmentManagerComponent>() : nullptr;
}

URSWeaponItemInstance* URSWeaponSlotsComponent::GetItemInSlot(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

URSWeaponItemInstance* URSWeaponSlotsComponent::FindItemByDefinition(const URSWeaponItemDefinition* Definition) const
{
	if (!Definition)
	{
		return nullptr;
	}

	for (URSWeaponItemInstance* Item : Slots)
	{
		if (Item && Item->GetDefinition() == Definition)
		{
			return Item;
		}
	}

	return nullptr;
}

bool URSWeaponSlotsComponent::HasAnyWeapon() const
{
	for (const URSWeaponItemInstance* Item : Slots)
	{
		if (Item)
		{
			return true;
		}
	}

	return false;
}

int32 URSWeaponSlotsComponent::FindAdjacentOccupiedSlot(int32 Direction) const
{
	const int32 Count = Slots.Num();
	if (Count == 0 || Direction == 0)
	{
		return INDEX_NONE;
	}

	const int32 Step = Direction > 0 ? 1 : -1;
	const int32 Start = Slots.IsValidIndex(ActiveSlotIndex) ? ActiveSlotIndex : 0;

	// At most one lap, stopping before the drawn slot comes round again.
	for (int32 Offset = 1; Offset < Count; ++Offset)
	{
		const int32 Candidate = ((Start + Step * Offset) % Count + Count) % Count;
		if (Slots[Candidate])
		{
			return Candidate;
		}
	}

	return INDEX_NONE;
}

URSWeaponItemInstance* URSWeaponSlotsComponent::GiveWeapon(const URSWeaponItemDefinition* Definition, bool bDraw)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogRS, Warning, TEXT("GiveWeapon called without authority on [%s]; ignored."), *GetNameSafe(Owner));
		return nullptr;
	}

	if (!Definition || !Definition->EquipmentDefinition)
	{
		UE_LOG(LogRS, Warning, TEXT("GiveWeapon on [%s]: [%s] has no equipment definition."), *GetNameSafe(Owner), *GetNameSafe(Definition));
		return nullptr;
	}

	if (FindItemByDefinition(Definition))
	{
		UE_LOG(LogRS, Log, TEXT("GiveWeapon on [%s]: already carries [%s]."), *GetNameSafe(Owner), *GetNameSafe(Definition));
		return nullptr;
	}

	int32 TargetSlot = Slots.IndexOfByKey(nullptr);
	if (TargetSlot == INDEX_NONE)
	{
		// Every slot is full: the new weapon takes the place of the one in the player's hands.
		TargetSlot = Slots.IsValidIndex(ActiveSlotIndex) ? ActiveSlotIndex : 0;
	}

	const bool bReplacesDrawnWeapon = (TargetSlot == ActiveSlotIndex);
	if (bReplacesDrawnWeapon)
	{
		HolsterDrawnWeapon();
	}

	RemoveItemFromSlot(TargetSlot);

	// Outered to the pawn, like the equipment instances: a UObject subobject of a component does not
	// replicate reliably.
	URSWeaponItemInstance* NewItem = NewObject<URSWeaponItemInstance>(Owner);
	NewItem->SetDefinition(Definition);
	NewItem->ResetStatsToInitial();

	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		AddReplicatedSubObject(NewItem);
	}

	Slots[TargetSlot] = NewItem;

	if (bDraw || bReplacesDrawnWeapon || ActiveSlotIndex == INDEX_NONE)
	{
		SetActiveSlot(TargetSlot);
	}
	else
	{
		OnSlotsChanged.Broadcast(this);
	}

	return NewItem;
}

void URSWeaponSlotsComponent::SetActiveSlot(int32 SlotIndex)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	URSWeaponItemInstance* Item = GetItemInSlot(SlotIndex);
	if (!Item)
	{
		return;
	}

	// Already in the player's hands: nothing to holster, nothing to draw.
	if (SlotIndex == ActiveSlotIndex && DrawnEquipment)
	{
		return;
	}

	HolsterDrawnWeapon();

	ActiveSlotIndex = SlotIndex;

	if (URSEquipmentManagerComponent* EquipmentManager = GetEquipmentManager())
	{
		// The item is the instigator, which is how the fire ability's cost and the reload reach the
		// ammo, and how the weapon instance poses its magazine.
		DrawnEquipment = EquipmentManager->EquipItem(Item->GetDefinition()->EquipmentDefinition, Item);
	}
	else
	{
		UE_LOG(LogRS, Warning, TEXT("[%s] has weapon slots but no equipment manager; nothing drawn."), *GetNameSafe(Owner));
	}

	OnSlotsChanged.Broadcast(this);
}

void URSWeaponSlotsComponent::HolsterDrawnWeapon()
{
	if (DrawnEquipment)
	{
		if (URSEquipmentManagerComponent* EquipmentManager = GetEquipmentManager())
		{
			EquipmentManager->UnequipItem(DrawnEquipment);
		}

		DrawnEquipment = nullptr;
	}
}

void URSWeaponSlotsComponent::RemoveItemFromSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex])
	{
		return;
	}

	if (IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(Slots[SlotIndex]);
	}

	Slots[SlotIndex] = nullptr;
}

void URSWeaponSlotsComponent::RemoveAllWeapons()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	HolsterDrawnWeapon();

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		RemoveItemFromSlot(SlotIndex);
	}

	ActiveSlotIndex = INDEX_NONE;
	OnSlotsChanged.Broadcast(this);
}

void URSWeaponSlotsComponent::UninitializeComponent()
{
	// The equipment manager tears down its own instances as it goes; this only has to stop holding
	// the drawn one and take the items out of replication.
	DrawnEquipment = nullptr;

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			RemoveItemFromSlot(SlotIndex);
		}
	}

	Super::UninitializeComponent();
}

bool URSWeaponSlotsComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (URSWeaponItemInstance* Item : Slots)
	{
		if (IsValid(Item))
		{
			bWroteSomething |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
		}
	}

	return bWroteSomething;
}

void URSWeaponSlotsComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// Anything given before replication was ready still has to be registered as a subobject.
	if (IsUsingRegisteredSubObjectList())
	{
		for (URSWeaponItemInstance* Item : Slots)
		{
			if (IsValid(Item))
			{
				AddReplicatedSubObject(Item);
			}
		}
	}
}

void URSWeaponSlotsComponent::OnRep_Slots()
{
	OnSlotsChanged.Broadcast(this);
}

void URSWeaponSlotsComponent::OnRep_ActiveSlotIndex()
{
	OnSlotsChanged.Broadcast(this);
}
