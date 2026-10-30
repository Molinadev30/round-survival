// Round Survival

#include "Weapons/RSWeaponItemInstance.h"

#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RSGameplayTags.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponSkin.h"

UWorld* URSWeaponItemInstance::GetWorld() const
{
	const APawn* OwningPawn = GetPawn();
	return OwningPawn ? OwningPawn->GetWorld() : nullptr;
}

void URSWeaponItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Definition);
	DOREPLIFETIME(ThisClass, StatTags);
	DOREPLIFETIME(ThisClass, SkinOverride);
	DOREPLIFETIME(ThisClass, UpgradeLevel);
}

APawn* URSWeaponItemInstance::GetPawn() const
{
	return Cast<APawn>(GetOuter());
}

void URSWeaponItemInstance::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.AddStack(Tag, StackCount);
	OnStatsChanged.Broadcast(this);
}

void URSWeaponItemInstance::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.RemoveStack(Tag, StackCount);
	OnStatsChanged.Broadcast(this);
}

int32 URSWeaponItemInstance::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTags.GetStackCount(Tag);
}

void URSWeaponItemInstance::OnRep_StatTags()
{
	// Runs after the fast array callbacks, so the count map is already current here.
	OnStatsChanged.Broadcast(this);
}

const URSWeaponSkin* URSWeaponItemInstance::GetSkin() const
{
	if (SkinOverride)
	{
		return SkinOverride;
	}

	return Definition ? Definition->Skin.Get() : nullptr;
}

void URSWeaponItemInstance::SetSkinOverride(const URSWeaponSkin* InSkin)
{
	if (SkinOverride == InSkin)
	{
		return;
	}

	SkinOverride = InSkin;
	OnSkinChanged.Broadcast(this);
}

void URSWeaponItemInstance::OnRep_SkinOverride()
{
	OnSkinChanged.Broadcast(this);
}

void URSWeaponItemInstance::ResetStatsToInitial()
{
	if (!Definition)
	{
		return;
	}

	// Topped up or drained to the starting value, so a stack that somehow ran over is put back too.
	for (const TPair<FGameplayTag, int32>& Initial : Definition->InitialStats)
	{
		SetStatTagStack(Initial.Key, Initial.Value);
	}

	OnStatsChanged.Broadcast(this);
}

void URSWeaponItemInstance::SetStatTagStack(FGameplayTag Tag, int32 Count)
{
	// Adding the difference keeps it one replicated change per tag.
	const int32 Difference = Count - StatTags.GetStackCount(Tag);
	if (Difference > 0)
	{
		StatTags.AddStack(Tag, Difference);
	}
	else if (Difference < 0)
	{
		StatTags.RemoveStack(Tag, -Difference);
	}
}

int32 URSWeaponItemInstance::GetFullReserveAmmo() const
{
	if (UpgradeLevel > 0 && Definition)
	{
		return Definition->Upgrade.ReserveAmmo;
	}

	const int32* InitialReserve = Definition ? Definition->InitialStats.Find(RSGameplayTags::Weapon_Ammo_Reserve) : nullptr;
	return InitialReserve ? *InitialReserve : 0;
}

void URSWeaponItemInstance::RefillReserveAmmo()
{
	const int32 Missing = GetFullReserveAmmo() - StatTags.GetStackCount(RSGameplayTags::Weapon_Ammo_Reserve);
	if (Missing > 0)
	{
		AddStatTagStack(RSGameplayTags::Weapon_Ammo_Reserve, Missing);
	}
}

void URSWeaponItemInstance::RefillAllAmmo()
{
	SetStatTagStack(RSGameplayTags::Weapon_Ammo_Magazine, StatTags.GetStackCount(RSGameplayTags::Weapon_MagazineSize));
	SetStatTagStack(RSGameplayTags::Weapon_Ammo_Reserve, GetFullReserveAmmo());
	OnStatsChanged.Broadcast(this);
}

bool URSWeaponItemInstance::IsReserveAmmoFull() const
{
	return StatTags.GetStackCount(RSGameplayTags::Weapon_Ammo_Reserve) >= GetFullReserveAmmo();
}

bool URSWeaponItemInstance::CanBeUpgraded() const
{
	return Definition && Definition->bUpgradable && UpgradeLevel == 0;
}

bool URSWeaponItemInstance::ApplyUpgrade()
{
	if (!CanBeUpgraded())
	{
		return false;
	}

	const FRSWeaponUpgrade& Upgrade = Definition->Upgrade;
	UpgradeLevel = 1;

	// A bigger magazine, loaded, and a bigger reserve, full: an upgraded weapon comes back ready.
	SetStatTagStack(RSGameplayTags::Weapon_MagazineSize, Upgrade.MagazineSize);
	SetStatTagStack(RSGameplayTags::Weapon_Ammo_Magazine, Upgrade.MagazineSize);
	SetStatTagStack(RSGameplayTags::Weapon_Ammo_Reserve, Upgrade.ReserveAmmo);
	OnStatsChanged.Broadcast(this);

	if (Upgrade.Skin)
	{
		SetSkinOverride(Upgrade.Skin);
	}

	return true;
}

void URSWeaponItemInstance::OnRep_UpgradeLevel()
{
	// The name the HUD shows comes with the level, and the HUD already listens for stat changes.
	OnStatsChanged.Broadcast(this);
}

FText URSWeaponItemInstance::GetDisplayName() const
{
	if (!Definition)
	{
		return FText::GetEmpty();
	}

	if (UpgradeLevel > 0 && !Definition->Upgrade.DisplayName.IsEmpty())
	{
		return Definition->Upgrade.DisplayName;
	}

	return Definition->DisplayName;
}

float URSWeaponItemInstance::GetDamageMultiplier() const
{
	return UpgradeLevel > 0 && Definition ? Definition->Upgrade.DamageMultiplier : 1.0f;
}

int32 URSWeaponItemInstance::GetRefillCost() const
{
	if (!Definition)
	{
		return 0;
	}

	return UpgradeLevel > 0 ? Definition->Upgrade.RefillCost : Definition->RefillCost;
}
