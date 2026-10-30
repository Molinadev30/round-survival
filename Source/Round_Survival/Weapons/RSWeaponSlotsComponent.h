// Round Survival

#pragma once

#include "Components/PawnComponent.h"

#include "RSWeaponSlotsComponent.generated.h"

class URSEquipmentInstance;
class URSEquipmentManagerComponent;
class URSWeaponItemDefinition;
class URSWeaponItemInstance;

DECLARE_MULTICAST_DELEGATE_OneParam(FRSWeaponSlotsChanged, class URSWeaponSlotsComponent* /*Slots*/);

/**
 * The weapons a player carries, and which one is in their hands.
 *
 * Lyra's quick bar trimmed to what the genre needs: a fixed number of slots, the items in them, and
 * the index of the drawn one. It decides *what* is drawn; the equipment manager does the drawing.
 * Drawing creates the weapon's equipment instance with the item as its instigator, holstering
 * destroys it, and the item — with its ammo — stays in the slot.
 *
 * On the pawn, not the controller or the player state: in this genre a player who dies loses their
 * weapons, and a respawned pawn starts again from its default weapons.
 *
 * Every change is server only. Clients receive the slots and the drawn index by replication and
 * hear about them through OnSlotsChanged, which is what the HUD will listen to.
 */
UCLASS(BlueprintType)
class ROUND_SURVIVAL_API URSWeaponSlotsComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URSWeaponSlotsComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Gives the player a weapon. Server only.
	 *
	 * It goes into the first empty slot; with every slot full it replaces the weapon in the player's
	 * hands, as buying a third weapon does in the genre. A weapon the player already carries is
	 * refused — topping one up is a refill, not a second copy.
	 *
	 * @param bDraw  Draw it immediately. Always the case when it replaces the drawn weapon, since
	 *               the hands cannot keep holding something that is no longer carried.
	 * @return The new item, or null if nothing was given.
	 */
	URSWeaponItemInstance* GiveWeapon(const URSWeaponItemDefinition* Definition, bool bDraw = true);

	/**
	 * Draws the weapon in a slot, holstering whatever was drawn. Server only, and immediate: the
	 * animation around it is the switch ability's business, not this component's.
	 */
	void SetActiveSlot(int32 SlotIndex);

	/** Holsters and drops every weapon. Server only. */
	void RemoveAllWeapons();

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	int32 GetNumSlots() const { return Slots.Num(); }

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	URSWeaponItemInstance* GetItemInSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	URSWeaponItemInstance* GetActiveItem() const { return GetItemInSlot(ActiveSlotIndex); }

	/** The carried copy of a weapon, or null. What a Wall Locker asks before selling or refilling. */
	URSWeaponItemInstance* FindItemByDefinition(const URSWeaponItemDefinition* Definition) const;

	/** Whether any slot holds a weapon. */
	bool HasAnyWeapon() const;

	/**
	 * The next occupied slot after the drawn one in a direction, wrapping round, or INDEX_NONE when
	 * no other slot is occupied. What the mouse wheel switches to.
	 */
	int32 FindAdjacentOccupiedSlot(int32 Direction) const;

	/** The slots or the drawn index changed, on any machine. */
	FRSWeaponSlotsChanged OnSlotsChanged;

	//~UObject interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	//~End of UObject interface

	//~UActorComponent interface
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void ReadyForReplication() override;
	//~End of UActorComponent interface

protected:
	/**
	 * How many weapons a player can carry. Data, so a station can raise it later without a second
	 * kind of slot.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ClampMin = 1))
	int32 NumSlots = 2;

private:
	URSEquipmentManagerComponent* GetEquipmentManager() const;

	/** Unequips whatever is drawn. Server only. */
	void HolsterDrawnWeapon();

	/** Takes an item out of its slot and out of replication. Server only. */
	void RemoveItemFromSlot(int32 SlotIndex);

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_ActiveSlotIndex();

	/** One entry per slot, null when empty. Sized to NumSlots on the server at initialisation. */
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<TObjectPtr<URSWeaponItemInstance>> Slots;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlotIndex)
	int32 ActiveSlotIndex = INDEX_NONE;

	/** The equipment instance of the drawn weapon. Server only: a client never unequips anything. */
	UPROPERTY()
	TObjectPtr<URSEquipmentInstance> DrawnEquipment;
};
