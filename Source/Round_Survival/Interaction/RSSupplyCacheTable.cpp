// Round Survival

#include "Interaction/RSSupplyCacheTable.h"

#include "Equipment/RSEquipmentDefinition.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponSlotsComponent.h"

bool URSSupplyCacheTable::IsEligible(const FRSSupplyCacheEntry& Entry, const URSWeaponSlotsComponent* Carrier) const
{
	// An item that cannot be drawn would be handed over and refused by the slots.
	if (Entry.Item == nullptr || Entry.Item->EquipmentDefinition == nullptr || Entry.Weight <= 0.0f)
	{
		return false;
	}

	return Carrier == nullptr || Carrier->FindItemByDefinition(Entry.Item) == nullptr;
}

const URSWeaponItemDefinition* URSSupplyCacheTable::Roll(const URSWeaponSlotsComponent* Carrier) const
{
	float TotalWeight = 0.0f;
	for (const FRSSupplyCacheEntry& Entry : Entries)
	{
		if (IsEligible(Entry, Carrier))
		{
			TotalWeight += Entry.Weight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Pick = FMath::FRandRange(0.0f, TotalWeight);
	const URSWeaponItemDefinition* Last = nullptr;

	for (const FRSSupplyCacheEntry& Entry : Entries)
	{
		if (!IsEligible(Entry, Carrier))
		{
			continue;
		}

		Last = Entry.Item;
		Pick -= Entry.Weight;

		if (Pick <= 0.0f)
		{
			return Entry.Item;
		}
	}

	// Float rounding at the very top of the range lands here: the last eligible entry owns it.
	return Last;
}

bool URSSupplyCacheTable::HasAnythingFor(const URSWeaponSlotsComponent* Carrier) const
{
	return Entries.ContainsByPredicate([this, Carrier](const FRSSupplyCacheEntry& Entry) { return IsEligible(Entry, Carrier); });
}

const URSWeaponItemDefinition* URSSupplyCacheTable::PickForDisplay(const URSWeaponItemDefinition* Previous) const
{
	TArray<const URSWeaponItemDefinition*, TInlineAllocator<16>> Candidates;
	for (const FRSSupplyCacheEntry& Entry : Entries)
	{
		if (Entry.Item && Entry.Item != Previous)
		{
			Candidates.AddUnique(Entry.Item);
		}
	}

	if (Candidates.IsEmpty())
	{
		return Previous;
	}

	return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}
