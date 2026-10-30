// Round Survival

#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/RSInteractable.h"

#include "RSPurchasable.generated.h"

class UStaticMeshComponent;

/**
 * Anything in the level that takes scrap and gives something back.
 *
 * Wall Lockers, Sector Gates, Supply Caches, Weapon Forges and the four Stations are all this
 * class with a different `OnPurchased`. Everything they share is here: a price, a prompt that
 * says what it is and what it costs, a refusal when the player cannot afford it, and a hook that
 * runs on the server once the purchase has been paid for.
 *
 * **It does not take the scrap.** The interaction ability's `URSAbilityCost_Scrap` does, on
 * commit, using the price this actor named. By the time `OnPurchased` runs the money is already
 * gone, which is why subclasses never check affordability again — and why a subclass that charged
 * its own scrap would charge twice.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API ARSPurchasable : public AActor, public IRSInteractable
{
	GENERATED_BODY()

public:
	ARSPurchasable();

	//~IRSInteractable interface
	virtual FText GetPrompt_Implementation(AActor* Interactor) const override;
	virtual float GetHoldSeconds_Implementation(AActor* Interactor) const override;
	virtual int32 GetScrapCost_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool IsRepeatable_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	//~End of IRSInteractable interface

protected:
	/**
	 * The purchase itself, already paid for. Server only.
	 *
	 * A Blueprint native event so content can build a simple purchasable — a door that opens, a
	 * light that comes on — without a C++ class, while the wall locker overrides it in C++.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "RS|Purchasable")
	void OnPurchased(AActor* Interactor);
	virtual void OnPurchased_Implementation(AActor* Interactor) {}

	/**
	 * Whether this is available at all, before price is considered.
	 *
	 * A gate that is already open, a locker with nothing to refill, a station already bought.
	 * Kept separate from affordability so the prompt can say which of the two is the problem.
	 * Power is not asked here: `bRequiresPower` answers it for every subclass, ahead of this.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "RS|Purchasable")
	bool IsAvailable(AActor* Interactor) const;
	virtual bool IsAvailable_Implementation(AActor* Interactor) const { return true; }

	/** Whether this interactor can pay the current price. */
	bool CanAfford(AActor* Interactor) const;

	/** False only while this needs the map's power and the power is off. */
	bool HasPower() const;

	/**
	 * What the prompt says is for sale: "Press {Key} for <this> [Cost: N]". `DisplayName` unless a
	 * subclass names what it sells itself — a Wall Locker names the weapon, or the weapon's ammo.
	 */
	virtual FText GetPromptName(AActor* Interactor) const;

	/** The whole prompt when `IsAvailable` refuses: no key and no price, because nothing is for sale. */
	virtual FText GetUnavailablePrompt(AActor* Interactor) const;

	/**
	 * False in a subclass that takes its name and price from what it sells, which hides
	 * `DisplayName` and `Cost` so nobody tunes a field that is never read.
	 */
	UPROPERTY()
	bool bHasFixedNameAndCost = true;

	/** What the prompt calls this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Purchasable", meta = (EditCondition = "bHasFixedNameAndCost", EditConditionHides))
	FText DisplayName;

	/** Scrap. Zero is free, which is legitimate: a gate can be opened by a switch rather than paid for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Purchasable", meta = (ClampMin = 0, EditCondition = "bHasFixedNameAndCost", EditConditionHides))
	int32 Cost = 500;

	/**
	 * Seconds the key must be held.
	 *
	 * Zero by default, and deliberately so: a purchase is a transaction, and a hold on it is a
	 * tax on the player's attention in the middle of a round. The hold exists for interactions
	 * where the wait is the cost, such as reviving.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Purchasable", meta = (ForceUnits = s, ClampMin = 0.0))
	float HoldSeconds = 0.0f;

	/**
	 * False in the Power Core, which cannot wait for itself. Hides `bRequiresPower`, and overrides it
	 * if it was set before.
	 */
	UPROPERTY()
	bool bCanRequirePower = true;

	/** Unusable until the map's Power Core is on. Until then the prompt says so instead of naming a price. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Purchasable", meta = (EditCondition = "bCanRequirePower", EditConditionHides))
	bool bRequiresPower = false;

	/** False, always, for anything that charges: a held key must not buy twice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Purchasable")
	bool bRepeatable = false;

	/** The thing the player sees and traces against. Subclasses set the mesh in data. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Purchasable")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
