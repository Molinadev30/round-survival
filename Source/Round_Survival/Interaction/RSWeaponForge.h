// Round Survival

#pragma once

#include "Interaction/RSPurchasable.h"

#include "RSWeaponForge.generated.h"

class URSWeaponItemInstance;

/**
 * Upgrades the weapon in the player's hands: hold, pay, and it comes back stronger.
 *
 * The forge holds no upgrade of its own. What a weapon becomes is data on the weapon, its
 * definition's `Upgrade`, and the upgrade belongs to the copy the player holds, its item instance:
 * the level, a bigger magazine and reserve, both filled, the upgraded name and finish, and a damage
 * multiplier the fire ability reads from the item. So the same forge upgrades every weapon by that
 * weapon's numbers, and the upgrade survives every holster with the rest of the item.
 *
 * **It takes both hands.** `InterruptsWeapon` is true, so the hold stops a burst or a reload, and
 * `RS.State.HandsBusy` refuses firing, reloading and switching weapons until it ends. Switching
 * matters most: the weapon upgraded when the hold completes has to be the one it started on.
 *
 * The weapon is asked about by `IsAvailable`, before anything is charged: nothing in hand, a weapon
 * the forge does not take, or one already upgraded, and the prompt says which. Every answer reads
 * replicated state, the slots and the item, so the owning client draws the prompt the server acts
 * on, and the server asks again in `OnPurchased`.
 *
 * Needs the map's power by default, as the genre has it.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSWeaponForge : public ARSPurchasable
{
	GENERATED_BODY()

public:
	ARSWeaponForge();

	//~IRSInteractable interface
	virtual bool InterruptsWeapon_Implementation(AActor* Interactor) const override { return true; }
	//~End of IRSInteractable interface

protected:
	//~ARSPurchasable interface
	virtual bool IsAvailable_Implementation(AActor* Interactor) const override;
	virtual void OnPurchased_Implementation(AActor* Interactor) override;
	virtual FText GetPromptName(AActor* Interactor) const override;
	virtual FText GetUnavailablePrompt(AActor* Interactor) const override;
	//~End of ARSPurchasable interface

	/** The actor's origin, on the floor. The forge stands on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Weapon Forge")
	TObjectPtr<USceneComponent> Origin;

private:
	/** The weapon in the interactor's hands, or null. */
	static URSWeaponItemInstance* FindWeaponInHand(AActor* Interactor);
};
