// Round Survival

#pragma once

#include "Engine/TimerHandle.h"
#include "Interaction/RSPurchasable.h"

#include "RSSupplyCache.generated.h"

class ARSSupplyCacheSpot;
class UBoxComponent;
class UGameplayEffect;
class URSSupplyCacheTable;
class URSWeaponAssemblyComponent;
class URSWeaponItemDefinition;
class URSWeaponSlotsComponent;

/** Where a Supply Cache is in its one cycle. */
UENUM(BlueprintType)
enum class ERSSupplyCachePhase : uint8
{
	/** Shut, and for sale. */
	Idle,

	/** Paid for and drawn; showing weapons while the player waits for the draw. */
	Rolling,

	/** Showing what was drawn, which only the buyer can take, for free, until it goes back in. */
	Offering,

	/** The roll ended with nothing: the buyer has been paid back and the cache is about to go. */
	Leaving,

	/** Gone from the level, already standing on its next spot, until it comes back there. */
	Away,
};

/**
 * A crate that sells a weapon nobody chose: pay, wait while it rolls, then take what it offers.
 *
 * **The draw happens on the server, the moment it is paid for**, from a `URSSupplyCacheTable`:
 * weighted, and leaving out what the buyer already carries. The result is kept on the server while
 * the cache rolls and only replicates when the roll ends, so no client learns it before it shows.
 * What every machine shows while it rolls is decoration, picked locally on a timer and never
 * replicated.
 *
 * **Taking the weapon is a second interaction, and a free one.** While the cache offers, its price
 * is zero for the buyer, and the interaction ability's scrap cost reads that price like any other.
 * Only the buyer can take it: everyone else sees the cache's name and nothing to press. The weapon
 * goes in a free slot, or replaces the one in the buyer's hands, the same as a Wall Locker's. Left
 * there for `OfferSeconds`, it goes back in, and the scrap is not returned.
 *
 * **It moves.** After `MinRollsBeforeMove` rolls in one place, each roll has a `MoveChance` of
 * drawing nothing: the roll plays out, the buyer gets back what they paid, and the cache leaves for
 * one of its `Spots`, the places the level lists for it, never anywhere else. It is gone for
 * `AwaySeconds` and comes back there, shut and for sale. Whether a roll leaves is decided with the
 * draw, at payment, and kept on the server like the weapon until the roll ends.
 *
 * **A sale puts one on every spot.** The Markdown power-up calls `StartSale` on the level's cache,
 * which spawns a sale cache on each of its spots that has none, for as long as the power-up lasts.
 * While it lasts no cache moves. When it ends the sale caches go, each once nobody is using it.
 *
 * A beam of light stands over it wherever it is, so players can find it again.
 *
 * Phase, buyer, the offered weapon and the spot replicate, so the owning client's prompt is the answer
 * the server acts on. Nothing ticks: the phases end on server timers.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSSupplyCache : public ARSPurchasable
{
	GENERATED_BODY()

public:
	ARSSupplyCache();

	//~IRSInteractable interface
	virtual int32 GetScrapCost_Implementation(AActor* Interactor) const override;
	//~End of IRSInteractable interface

	UFUNCTION(BlueprintPure, Category = "RS|Supply Cache")
	ERSSupplyCachePhase GetPhase() const { return Phase; }

	/** The weapon on offer, and only while it is: null while the cache rolls, even on the server. */
	UFUNCTION(BlueprintPure, Category = "RS|Supply Cache")
	const URSWeaponItemDefinition* GetOfferedItem() const { return OfferedItem; }

	/** Who paid for the cycle under way, or null when there is none. */
	UFUNCTION(BlueprintPure, Category = "RS|Supply Cache")
	AActor* GetBuyer() const { return Buyer; }

	/** The spot it stands on, or null while it is still where the level placed it. */
	UFUNCTION(BlueprintPure, Category = "RS|Supply Cache")
	ARSSupplyCacheSpot* GetCurrentSpot() const { return CurrentSpot; }

	/** Server. The next roll paid for draws nothing and the cache moves, if it has somewhere to go. Debug. */
	void ForceMoveOnNextRoll() { bForceMoveOnNextRoll = true; }

	/**
	 * Server. For Seconds, a sale cache stands on each of this cache's spots that has no cache, and
	 * no cache moves. A second sale while one is on starts the time over and fills any spot left empty.
	 */
	void StartSale(float Seconds);

	/** Server. Whether a sale spawned this one, to go when it ends. A sale cache never moves and never holds a sale. */
	bool IsSaleCache() const { return bSaleCache; }

protected:
	//~AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface

	//~ARSPurchasable interface
	virtual bool IsAvailable_Implementation(AActor* Interactor) const override;
	virtual void OnPurchased_Implementation(AActor* Interactor) override;
	virtual FText GetPromptName(AActor* Interactor) const override;
	virtual FText GetUnavailablePrompt(AActor* Interactor) const override;
	//~End of ARSPurchasable interface

	/** The actor's origin, on the floor. The crate stands on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache")
	TObjectPtr<USceneComponent> Origin;

	/** Where the middle of the floating weapon goes, over the crate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache")
	TObjectPtr<USceneComponent> DisplayAnchor;

	/**
	 * The weapon over the crate, built by the same component that builds one in a player's hands, so
	 * what is offered is what the buyer walks away with. Empty while the cache is shut. Centred on
	 * `DisplayAnchor`, so its own location is derived.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache")
	TObjectPtr<URSWeaponAssemblyComponent> DisplayMesh;

	/**
	 * Blocks the navigation where the crate stands, and nowhere else, so enemies walk round it on
	 * whichever spot it is: a dynamic obstacle fitted to the crate, which follows it when it moves
	 * and leaves the navigation while it is away. The crate itself stays out of the navigation, or
	 * its first spot would stay carved out of the built navmesh after it left.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache")
	TObjectPtr<UBoxComponent> NavBlocker;

	/**
	 * The beam of light over the crate, seen from across the level. No collision and no shadow; its
	 * mesh, size and material are content.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache")
	TObjectPtr<UStaticMeshComponent> Beam;

	/** What a buyer pays while the Markdown power-up is on them, instead of `Cost`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache", meta = (ClampMin = 0))
	int32 MarkdownCost = 10;

	/** What this cache can hand out. Several caches can share one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache")
	TObjectPtr<const URSSupplyCacheTable> Table;

	/** Seconds from paying to seeing what was drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache", meta = (ForceUnits = s, ClampMin = 0.0))
	float RollSeconds = 4.0f;

	/** Seconds the buyer has to take the weapon before it goes back in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache", meta = (ForceUnits = s, ClampMin = 0.1))
	float OfferSeconds = 12.0f;

	/** Seconds between the weapons shown while it rolls. Decoration only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache", meta = (ForceUnits = s, ClampMin = 0.02))
	float RollDisplayInterval = 0.15f;

	/** The places it can move to. It never goes anywhere else, and never stays where it was. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RS|Supply Cache|Moving")
	TArray<TObjectPtr<ARSSupplyCacheSpot>> Spots;

	/** Rolls in one place that never move it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache|Moving", meta = (ClampMin = 0))
	int32 MinRollsBeforeMove = 4;

	/** The chance that a roll after those moves it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache|Moving", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float MoveChance = 0.15f;

	/** Seconds it stands empty, saying it is moving, before it goes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache|Moving", meta = (ForceUnits = s, ClampMin = 0.0))
	float LeaveSeconds = 2.0f;

	/** Seconds it is gone before it is back, on its new spot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Supply Cache|Moving", meta = (ForceUnits = s, ClampMin = 0.0))
	float AwaySeconds = 8.0f;

	/**
	 * Pays the buyer back when a roll moves it: the effect a purchase charges through, handed the
	 * price as a positive amount. Straight onto the balance, so an income multiplier never changes
	 * what comes back.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Supply Cache|Moving")
	TSubclassOf<UGameplayEffect> RefundEffect;

private:
	static URSWeaponSlotsComponent* FindWeaponSlots(AActor* Interactor);

	bool IsBuyer(const AActor* Interactor) const;

	/** Server. Draws for the buyer and starts the roll. */
	void StartRoll(AActor* InBuyer, URSWeaponSlotsComponent& Slots);

	/** Server. The roll is over: what was drawn goes on offer. */
	void HandleRollFinished();

	/** Server. Nobody took it in time; it goes back in. */
	void HandleOfferExpired();

	/** Server. The buyer takes what is on offer. */
	void HandOverOffer(AActor* Interactor, URSWeaponSlotsComponent& Slots);

	/** Server. Back to shut and for sale, whatever ended the cycle. */
	void ResetCycle();

	/** Server. Whether a roll paid for now moves the cache. Asked once, at payment. */
	bool ShouldMoveOnThisRoll();

	/** Server. A spot to go to, never the one it is on, or null when there is none. */
	ARSSupplyCacheSpot* PickNextSpot() const;

	/** Server. The roll drew nothing: the buyer is paid back and the cache says it is moving. */
	void StartLeaving();

	/** Server. Pays the buyer back what they paid for the roll. */
	void RefundBuyer();

	/** Server. The sale is over: its caches go, the ones in use once their buyer is done. */
	void EndSale();

	/** Server. A sale cache going: now if nobody is using it, or at the end of the cycle under way. */
	void Retire();

	/** Server. Whether a cache stands on Spot, this one or a sale cache. */
	bool IsSpotTaken(const ARSSupplyCacheSpot* Spot) const;

	/** Server. It goes: onto its next spot, out of sight. */
	void HandleLeft();

	/** Server. Back, on its new spot, shut and for sale. */
	void HandleArrived();

	/** Server. Sets the phase and applies it here; clients apply it from the rep notify. */
	void SetPhase(ERSSupplyCachePhase NewPhase);

	/** Every machine. What the phase means for the world: away is unseen, untouchable and off the navigation. */
	void ApplyPhase();

	/** Every machine. Stands the cache on CurrentSpot. */
	void MoveToCurrentSpot();

	/** Sizes NavBlocker to the crate, so the cache has one shape and not two to keep in step. */
	void FitNavBlockerToMesh();

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION()
	void OnRep_CurrentSpot();

	UFUNCTION()
	void OnRep_OfferedItem();

	/** What this machine shows for the phase. Every machine but a dedicated server, which shows nothing. */
	void RefreshPresentation();

	void ShowNextRollWeapon();
	void ShowWeapon(const URSWeaponItemDefinition* Item);

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ERSSupplyCachePhase Phase = ERSSupplyCachePhase::Idle;

	/** Set when the roll ends, not when it starts, so the draw is not sent before it is shown. */
	UPROPERTY(ReplicatedUsing = OnRep_OfferedItem)
	TObjectPtr<const URSWeaponItemDefinition> OfferedItem;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> Buyer;

	/** Set as it goes away, so every machine has moved it before it shows again. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentSpot)
	TObjectPtr<ARSSupplyCacheSpot> CurrentSpot;

	/** Rolls paid for since it last arrived. Server. */
	int32 RollsHere = 0;

	/** The roll under way draws nothing and moves the cache. Decided at payment, on the server only. */
	bool bLeavingThisRoll = false;

	bool bForceMoveOnNextRoll = false;

	/** What the buyer of the roll under way paid, which a move hands back. Server. */
	int32 PaidCost = 0;

	/** A sale is on: nothing moves. Server. */
	bool bSaleOn = false;

	/** Spawned by a sale, and goes when it ends. Server. */
	bool bSaleCache = false;

	/** A sale cache whose sale has ended, waiting for its buyer to be done. Server. */
	bool bRetiring = false;

	/** The caches this one's sale put on its spots. Server. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ARSSupplyCache>> SaleCaches;

	FTimerHandle SaleTimerHandle;

	/** The draw, on the server only, from the purchase until the roll ends. */
	UPROPERTY(Transient)
	TObjectPtr<const URSWeaponItemDefinition> DrawnItem;

	/** What the display shows now, on this machine. */
	UPROPERTY(Transient)
	TObjectPtr<const URSWeaponItemDefinition> ShownItem;

	FTimerHandle PhaseTimerHandle;
	FTimerHandle RollDisplayTimerHandle;
};
