// Round Survival

#pragma once

#include "Interaction/RSPurchasable.h"

#include "RSPowerCore.generated.h"

class UPointLightComponent;

/**
 * The switch that turns the map's power on, once, for everyone.
 *
 * Free by default and bought through the interaction ability like anything else, so turning it on
 * is a server decision with no code of its own. **It does not hold the power.** `ARSGameState`
 * does, and the core only writes it: what needs power — anything with `bRequiresPower` — asks the
 * match, never the core, and the core's own light follows the same delegate as everyone else.
 *
 * One core per map is the design, but nothing breaks with two: the first one used turns the power
 * on and both go quiet.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSPowerCore : public ARSPurchasable
{
	GENERATED_BODY()

public:
	ARSPowerCore();

	//~IRSInteractable interface
	virtual FText GetPrompt_Implementation(AActor* Interactor) const override;
	//~End of IRSInteractable interface

protected:
	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	//~ARSPurchasable interface
	virtual bool IsAvailable_Implementation(AActor* Interactor) const override;
	virtual void OnPurchased_Implementation(AActor* Interactor) override;
	//~End of ARSPurchasable interface

	/** The actor's origin, on the floor. The core stands on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Power Core")
	TObjectPtr<USceneComponent> Origin;

	/** Off until the power is on, on every machine. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Power Core")
	TObjectPtr<UPointLightComponent> PowerLight;

private:
	/** Shows the power on the core itself. Run from the game state's delegate, and once on begin play. */
	void HandlePowerChanged(bool bPowerOn);

	bool IsPowerOn() const;

	FDelegateHandle PowerChangedHandle;
};
