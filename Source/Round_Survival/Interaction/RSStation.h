// Round Survival

#pragma once

#include "Interaction/RSPurchasable.h"

#include "RSStation.generated.h"

class URSStationDefinition;

/**
 * A buff station: pay, and keep the buff until you go down.
 *
 * One class for all four. What a placed station sells is its `Station` definition — name, price,
 * effect and tag — so the actor only answers the questions every purchasable answers: what is this
 * called, what does it cost, can this player have it. A player who already has the station is
 * refused through `IsAvailable`, before anything is charged, and the answer reads the replicated tag,
 * so the owning client draws the same prompt the server will act on.
 *
 * Needs the map's power by default, as the genre has it.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSStation : public ARSPurchasable
{
	GENERATED_BODY()

public:
	ARSStation();

	//~IRSInteractable interface
	virtual int32 GetScrapCost_Implementation(AActor* Interactor) const override;
	virtual bool InterruptsWeapon_Implementation(AActor* Interactor) const override { return true; }
	//~End of IRSInteractable interface

	UFUNCTION(BlueprintPure, Category = "RS|Station")
	const URSStationDefinition* GetStation() const { return Station; }

protected:
	//~ARSPurchasable interface
	virtual bool IsAvailable_Implementation(AActor* Interactor) const override;
	virtual void OnPurchased_Implementation(AActor* Interactor) override;
	virtual FText GetPromptName(AActor* Interactor) const override;
	//~End of ARSPurchasable interface

	/** The actor's origin, on the floor. The station stands on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Station")
	TObjectPtr<USceneComponent> Origin;

	/** What this station sells. Per placed station: the same Blueprint is any of the four. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Station")
	TObjectPtr<const URSStationDefinition> Station;
};
