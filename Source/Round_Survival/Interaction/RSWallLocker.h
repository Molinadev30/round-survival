// Round Survival

#pragma once

#include "Interaction/RSPurchasable.h"

#include "RSWallLocker.generated.h"

class URSWeaponAssemblyComponent;
class URSWeaponItemDefinition;
class URSWeaponItemInstance;
class URSWeaponSlotsComponent;

/**
 * A weapon on a wall, for sale.
 *
 * What it sells depends on who is asking. A player who does not carry the weapon buys it: the
 * item goes into a free slot, or replaces the drawn weapon when every slot is full, and is drawn.
 * A player who does carry it pays the lower refill price to have its reserve topped up to the
 * definition's starting value; the magazine is left as it is, so ammo bought with an empty
 * magazine still has to be reloaded. A player whose reserve is already full is refused through
 * `IsAvailable`, before anything is charged, and the prompt says why.
 *
 * Both prices and the weapon on the wall come from the item definition, so a locker is configured
 * by naming a weapon and nothing else. Every decision reads replicated state — the slots and the
 * item's stat stacks — so the owning client draws the same prompt the server will act on.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSWallLocker : public ARSPurchasable
{
	GENERATED_BODY()

public:
	ARSWallLocker();

	//~IRSInteractable interface
	virtual int32 GetScrapCost_Implementation(AActor* Interactor) const override;
	//~End of IRSInteractable interface

	UFUNCTION(BlueprintPure, Category = "RS|Wall Locker")
	const URSWeaponItemDefinition* GetWeaponItem() const { return WeaponItem; }

protected:
	//~AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	//~End of AActor interface

	//~ARSPurchasable interface
	virtual bool IsAvailable_Implementation(AActor* Interactor) const override;
	virtual void OnPurchased_Implementation(AActor* Interactor) override;
	virtual FText GetPromptName(AActor* Interactor) const override;
	virtual FText GetUnavailablePrompt(AActor* Interactor) const override;
	//~End of ARSPurchasable interface

	/** The weapon this locker sells. Per placed locker: the same Blueprint hangs any weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Wall Locker")
	TObjectPtr<const URSWeaponItemDefinition> WeaponItem;

	/**
	 * Where the middle of the weapon goes: in front of the panel, at its centre. Placed by the
	 * Blueprint, which shapes the panel; a placed locker can move it, and that holds for any weapon
	 * it is given, because the weapon centres itself on it.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Wall Locker")
	TObjectPtr<USceneComponent> DisplayAnchor;

	/**
	 * The weapon on the wall: the item's assembly, with every part, wearing the item's skin — the
	 * same component that builds the weapon in a player's hands, so the wall shows what the player
	 * walks away with. Centred on `DisplayAnchor` (see URSWeaponAssemblyComponent::bCentreOnAttachParent),
	 * so its own location is derived and never set by hand.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Wall Locker")
	TObjectPtr<URSWeaponAssemblyComponent> DisplayMesh;

private:
	/**
	 * Builds the item's assembly and puts its skin on. Run on construction, so the editor shows
	 * it as soon as a weapon is named, and again on BeginPlay, because a placed actor's
	 * construction is baked into the level and does not run when the game loads it: without the
	 * second pass the wall would keep whatever finish the level was last saved with.
	 */
	void RefreshDisplay();

	static URSWeaponSlotsComponent* FindWeaponSlots(AActor* Interactor);

	/** The interactor's copy of this locker's weapon, or null when they do not carry it. */
	URSWeaponItemInstance* FindCarriedCopy(AActor* Interactor) const;

	FText GetWeaponName() const;
};
