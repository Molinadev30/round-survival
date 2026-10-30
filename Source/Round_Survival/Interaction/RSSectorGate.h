// Round Survival

#pragma once

#include "Interaction/RSPurchasable.h"

#include "RSSectorGate.generated.h"

class ARSZone;
class UBoxComponent;

/**
 * A door, bought once, that lets the match into another part of the map.
 *
 * Buying it does two things that are kept apart on purpose. The gate itself opens — the panel stops
 * blocking, sinks out of the way and leaves the navigation — and the zones it names open, which is
 * what lets their spawners join the round. The gate's own state is not the zone's: two gates into
 * the same room are two purchases, and buying one leaves the other shut, as the genre has it.
 *
 * **Collision has to agree on every machine**, because a client predicts its own movement against
 * the panel. So the gate is always relevant, like the zones it opens, and `bOpened` is applied by
 * the same function on the server and in the rep notify.
 *
 * **The navigation is cut by a modifier, not by the panel.** The panel never affects navigation, so
 * the static build runs straight through the doorway; `NavBlocker`, a dynamic obstacle fitted to the
 * panel's bounds, marks that strip as null area until the gate opens and then leaves the navigation
 * system. Only the modifier is rebuilt at runtime, which is what `DynamicModifiersOnly` on the
 * navmesh allows, and enemies path round a shut gate instead of standing against it.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSSectorGate : public ARSPurchasable
{
	GENERATED_BODY()

public:
	ARSSectorGate();

	/** Bought and open. Replicated; authoritative on the server. */
	UFUNCTION(BlueprintPure, Category = "RS|Sector Gate")
	bool IsOpened() const { return bOpened; }

	//~AActor interface
	virtual void Tick(float DeltaSeconds) override;
	//~End of AActor interface

protected:
	//~AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface

	//~ARSPurchasable interface
	virtual bool IsAvailable_Implementation(AActor* Interactor) const override;
	virtual void OnPurchased_Implementation(AActor* Interactor) override;
	//~End of ARSPurchasable interface

	UFUNCTION()
	void OnRep_Opened();

	/** The zones behind this gate, opened with it. Usually one; empty is a door that only opens. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RS|Sector Gate")
	TArray<TObjectPtr<ARSZone>> ZonesToOpen;

	/** How long the panel takes to sink out of the doorway. Cosmetic: it stops blocking on the first frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Sector Gate", meta = (ForceUnits = s, ClampMin = 0.0))
	float OpenSeconds = 1.0f;

	/** The actor's origin, on the floor of the doorway. The panel hangs from it, so it can move. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Sector Gate")
	TObjectPtr<USceneComponent> Origin;

	/** The null-area strip the panel cuts out of the navigation while shut. Fitted to the panel; never collides. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Sector Gate")
	TObjectPtr<UBoxComponent> NavBlocker;

private:
	/**
	 * Puts the gate in its open state: no collision, no navigation, and the panel on its way down.
	 * Run on the server when bought and on clients from the rep notify. A gate that is already open
	 * when it begins play — a client that arrives late — is put straight at the end.
	 */
	void ApplyOpened(bool bAnimate);

	/** Sizes `NavBlocker` to the panel, so the gate has one shape and not two to keep in step. */
	void FitNavBlockerToPanel();

	UPROPERTY(ReplicatedUsing = OnRep_Opened)
	bool bOpened = false;

	/** Where the panel stands shut, relative to the origin, and how far it has sunk. */
	FVector PanelClosedLocation = FVector::ZeroVector;
	float PanelDrop = 0.0f;
	float OpenElapsed = 0.0f;
};
