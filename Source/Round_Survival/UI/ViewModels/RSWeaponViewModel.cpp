// Round Survival

#include "UI/ViewModels/RSWeaponViewModel.h"

#include "RSGameplayTags.h"
#include "Weapons/RSWeaponItemInstance.h"

void URSWeaponViewModel::SetItem(const URSWeaponItemInstance* Item)
{
	UE_MVVM_SET_PROPERTY_VALUE(WeaponVisibility, Item ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (!Item)
	{
		CachedItem.Reset();
		return;
	}

	// The name changes with the item, and with the Weapon Forge's upgrade of the same item.
	if (CachedItem.Get() != Item || CachedUpgradeLevel != Item->GetUpgradeLevel())
	{
		CachedItem = Item;
		CachedUpgradeLevel = Item->GetUpgradeLevel();
		CachedMagazine = INDEX_NONE;
		CachedReserve = INDEX_NONE;

		UE_MVVM_SET_PROPERTY_VALUE(WeaponName, Item->GetDisplayName());
	}

	const int32 Magazine = Item->GetStatTagStackCount(RSGameplayTags::Weapon_Ammo_Magazine);
	if (Magazine != CachedMagazine)
	{
		CachedMagazine = Magazine;
		UE_MVVM_SET_PROPERTY_VALUE(MagazineText, FText::AsNumber(Magazine));
	}

	const int32 Reserve = Item->GetStatTagStackCount(RSGameplayTags::Weapon_Ammo_Reserve);
	if (Reserve != CachedReserve)
	{
		CachedReserve = Reserve;
		UE_MVVM_SET_PROPERTY_VALUE(ReserveText, FText::AsNumber(Reserve));
	}
}
