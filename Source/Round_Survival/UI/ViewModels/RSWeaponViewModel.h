// Round Survival

#pragma once

#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "RSWeaponViewModel.generated.h"

class URSWeaponItemInstance;

/**
 * The drawn weapon: its name and the two ammo counts.
 *
 * Reads an item it is handed and holds text ready to bind. Which item that is, and when it
 * changed, is URSHUDSubsystem's business.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSWeaponViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rereads everything from the item. Null — no pawn, or empty hands — hides the readout. */
	void SetItem(const URSWeaponItemInstance* Item);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Weapon", meta = (AllowPrivateAccess = true))
	FText WeaponName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Weapon", meta = (AllowPrivateAccess = true))
	FText MagazineText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Weapon", meta = (AllowPrivateAccess = true))
	FText ReserveText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Weapon", meta = (AllowPrivateAccess = true))
	ESlateVisibility WeaponVisibility = ESlateVisibility::Collapsed;

	/** What the texts were last built from. FText compares by identity, so rebuilding unchanged text would notify every binding. */
	TWeakObjectPtr<const URSWeaponItemInstance> CachedItem;
	int32 CachedUpgradeLevel = INDEX_NONE;
	int32 CachedMagazine = INDEX_NONE;
	int32 CachedReserve = INDEX_NONE;
};
