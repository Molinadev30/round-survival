// Round Survival

#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/RSInteractable.h"

#include "RSBarricade.generated.h"

class ARSBarricade;
class UArrowComponent;
class UBoxComponent;
class URSBarricadeWindowLayout;
class UStaticMesh;
class UGameplayEffect;
class UMaterialInterface;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FRSBarricadeBoardsChanged, ARSBarricade* /*Barricade*/, int32 /*BoardCount*/);

/**
 * A boarded window. The one thing on the map that pays a player for standing still in the open.
 *
 * Enemies tear its boards off to get in; a player puts them back one at a time. Repairing is a
 * repeatable hold, and **each board pays the moment it lands** rather than at the end of the
 * hold — so letting go halfway keeps whatever was rebuilt, which is the rhythm the genre runs
 * on: a player low on scrap walks to a window and earns their way back into the round.
 *
 * **Not an `ARSPurchasable`.** Repair costs nothing and pays, so `GetScrapCost` stays at the
 * interface's zero and the award goes out from here through the same `GE_RS_ScrapAward` the
 * kills use — an income buff raises a repair like it raises everything else.
 *
 * The barricade stops accepting the interaction at `MaxBoards` rather than making the player
 * watch a number: `CanInteract` goes false, and the interaction ability's repeat loop asks it
 * before every step, so a held key simply stops when the window is full.
 *
 * Enemies queue on it through `JoinQueue`, and it hands its places out. Slots are server state and are deliberately not
 * replicated — only the server's AI reserves, and a client has no use for who is standing where.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSBarricade : public AActor, public IRSInteractable
{
	GENERATED_BODY()

public:
	ARSBarricade(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** How many boards are up. Replicated; authoritative on the server. */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	int32 GetBoardCount() const { return BoardCount; }

	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	int32 GetMaxBoards() const { return MaxBoards; }

	/** No boards left: the way in is open. What an enemy does about that is 7.4's business. */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	bool IsBreached() const { return BoardCount <= 0; }

	/**
	 * Tears one board off. Server only. Returns false when there was nothing left to tear.
	 *
	 * The enemy's barricade attack ability calls this; nothing else should.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Barricade")
	bool RemoveBoard(AActor* Attacker);

	/**
	 * Puts every board back at once. Server only. Pays nobody: whatever did it pays for itself,
	 * the way the Fortify power-up does.
	 */
	void RepairFully();

	/**
	 * Puts an enemy on this window's queue for a place to stand and attack from. Server only.
	 *
	 * **The window hands the places out, not the enemies.** Every free place goes to the queued
	 * enemy nearest it, at once: when an enemy joins, and when a place is given back. Nobody waits
	 * on a timer and nobody takes a place out of turn, so a place never stands empty while there
	 * is somebody queued, and the one that gets it is the one with the shortest walk. Only a place
	 * holder tears boards; the rest wait, which is the window-queueing the genre is built on.
	 *
	 * Idempotent: joining again, or while holding a place, changes nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Barricade")
	void JoinQueue(AActor* Claimant);

	/** Takes an enemy off the queue, if it is on it. A place it holds is not given back: see ReleaseSlot. */
	UFUNCTION(BlueprintCallable, Category = "RS|Barricade")
	void LeaveQueue(AActor* Claimant);

	/**
	 * Gives a place back, and hands it straight to the nearest queued enemy. Safe to call with a
	 * place the claimant no longer holds: only the holder frees it.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Barricade")
	void ReleaseSlot(int32 SlotIndex, AActor* Claimant);

	/** The place an enemy holds, or -1. The window's answer, which is the only one there is. */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	int32 GetSlotHeldBy(const AActor* Claimant) const;

	/**
	 * Claims the right to climb through. Server only. Returns false when someone else is inside.
	 *
	 * **One at a time, and that is the point.** A breached window is a bottleneck a player can
	 * still play: enemies file through it rather than pouring over the sill in a sheet, which is
	 * what gives a held position any value once the boards are gone. It is also the honest thing
	 * to do with a warped montage — two of them skewed onto the same sill at once would walk
	 * through one another.
	 *
	 * Idempotent by claimant, like `TryReserveSlot`: asking twice returns true rather than
	 * deadlocking a task that re-enters its state.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Barricade")
	bool TryReserveTransit(AActor* Claimant);

	/** Gives the crossing back. Safe to call from anyone; only the holder actually frees it. */
	UFUNCTION(BlueprintCallable, Category = "RS|Barricade")
	void ReleaseTransit(AActor* Claimant);

	/** True while someone is climbing through. */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	bool IsTransitBusy() const { return TransitHolder.IsValid(); }

	/**
	 * True while someone other than `Claimant` holds the crossing.
	 *
	 * The question an enemy that already reserved the crossing asks: it takes the claim before it
	 * steps up to the sill, so by the time it climbs the window is busy — with itself.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	bool IsTransitHeldByOther(const AActor* Claimant) const { return TransitHolder.IsValid() && TransitHolder.Get() != Claimant; }

	/** Where a slot holder stands. World space. */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	FTransform GetSlotTransform(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	int32 GetSlotCount() const { return AttackSlots.Num(); }

	/** Height of the sill an enemy vaults, measured from the ground. */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	float GetSillHeight() const { return SillHeight; }

	/**
	 * Where a vaulting enemy plants on the way in: the top of the sill, on the outside face, in
	 * the lane of the slot it climbs from.
	 *
	 * **Each slot climbs through its own lane.** The enemy vaults from where it already stands,
	 * so nobody has to walk across the window — through the enemy holding the middle slot — to a
	 * shared start point. A side slot sits wider than a capsule fits through the opening, so its
	 * lane is pulled in to `VaultLaneLimit`; the warp takes up that short sideways step during the
	 * climb. A slot of `INDEX_NONE` is the middle lane.
	 *
	 * This and `GetVaultBackLedge` are the two points the traversal montage is warped onto, and
	 * both come out of `SillHeight` rather than out of a component someone can drag: a window
	 * whose warp target disagrees with the sill it is drawn at is a vault through the brickwork.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	FVector GetVaultFrontLedge(int32 SlotIndex = -1) const;

	/**
	 * Where the same enemy lands: on the ground, `DistanceInside` past the inner face, in the same
	 * lane. How far in is the vault's to say — it depends on the clip, not on the window.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	FVector GetVaultBackLedge(int32 SlotIndex, float DistanceInside) const;

	/** How far across the window a slot's vault lane runs, from the middle. */
	float GetVaultLaneOffset(int32 SlotIndex) const;

	/** The shared window layout this barricade is built from. */
	const URSBarricadeWindowLayout* GetWindowLayout() const { return WindowLayout; }

	/**
	 * Where the enemy in `SlotIndex` puts its hand when it reaches through, in world space. False
	 * when the layout has no point for that slot. The same point whatever the board count; see the
	 * layout's `ReachPoints`.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	bool GetReachPoint(int32 SlotIndex, FVector& OutLocation) const;

	/** Degrees the enemy in `SlotIndex` turns while reaching, on top of facing the window. See the layout's `ReachPoints`. */
	float GetReachYawOffset(int32 SlotIndex) const;

	/** Whether the enemy in `SlotIndex` reaches with the other arm. */
	bool IsReachMirrored(int32 SlotIndex) const;

	/** Horizontal direction from outside to inside: the way a reach, a vault and a swing through the window point. */
	UFUNCTION(BlueprintPure, Category = "RS|Barricade")
	FVector GetInwardDirection() const { return -GetActorForwardVector().GetSafeNormal2D(); }

	/** Fires on every machine when the count settles, the server included. */
	FRSBarricadeBoardsChanged OnBoardsChanged;

	//~IRSInteractable interface
	virtual FText GetPrompt_Implementation(AActor* Interactor) const override;
	virtual float GetHoldSeconds_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool IsRepeatable_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	//~End of IRSInteractable interface

protected:
	//~AActor interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface

	UFUNCTION()
	void OnRep_BoardCount();

	/** Shows and hides board meshes to match the count. Runs on both sides, off the same number. */
	void ApplyBoardVisuals();

	/** Places the boards and the sill, lintel and jambs around them. Editor and runtime alike. */
	void ApplyWindowLayout();

	/** Pays the repairer for one board. Server only. */
	void AwardRepairScrap(AActor* Interactor);

	/**
	 * How high the sill stands off the ground, in centimetres.
	 *
	 * A hundred because that is the obstacle the traversal animation was authored over, measured
	 * off its own root track: the feet clear the ledge at 103 cm. Motion warping will stretch the
	 * clip onto a different number, but every centimetre of difference is a centimetre of skew in
	 * the one direction a player's eye is good at catching, so the geometry is built to the
	 * animation rather than the animation bent to the geometry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade|Window", meta = (ForceUnits = cm, ClampMin = 0.0))
	float SillHeight = 100.0f;

	/**
	 * How far off the middle of the opening a vault lane may run.
	 *
	 * The opening is 115 cm across and an enemy's capsule 48, so a body climbing through has to
	 * stay within about 33 cm of the middle; 30 leaves a margin against the jambs. Side slots stand
	 * 45 out, so their climbs drift 15 cm inward on the way over.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade|Window", meta = (ForceUnits = cm, ClampMin = 0.0))
	float VaultLaneLimit = 30.0f;

	/**
	 * The shared window layout. Every barricade points at the same asset; see its class comment.
	 *
	 * Left unset, the boards fall back to even spacing above the sill, so an unconfigured
	 * barricade is still a working barricade rather than six planks in a heap at the origin.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barricade|Window")
	TObjectPtr<URSBarricadeWindowLayout> WindowLayout;

	/** The plank. Set in the Blueprint; the engine cube stands in while it is unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barricade|Window")
	TObjectPtr<UStaticMesh> BoardMesh;

	/**
	 * Over the plank mesh's own material. Left unset, the mesh's. The plank is borrowed from the
	 * level's wall pieces, so its own material is the wall's; this is how a bullet finds wood in it,
	 * through the physical material, while it still looks like the wall around it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barricade|Window")
	TObjectPtr<UMaterialInterface> BoardMaterial;

	/** The most boards this window holds. Six is the genre's number and the mesh count's ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade", meta = (ClampMin = 1, ClampMax = 6))
	int32 MaxBoards = 6;

	/** How many are up at the start of the match. Full, normally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade", meta = (ClampMin = 0, ClampMax = 6))
	int32 StartingBoards = 6;

	/** Seconds of hold per board. One press restores board after board at this rate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade", meta = (ForceUnits = s, ClampMin = 0.0))
	float SecondsPerBoard = 0.6f;

	/** Scrap for each board put back. Paid on the frame it lands, not when the hold ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade", meta = (ClampMin = 0.0))
	float ScrapPerBoard = 10.0f;

	/**
	 * The award effect, `GE_RS_ScrapAward`. Set in the Blueprint, like the enemy's.
	 *
	 * The same asset the kills pay through, so income multipliers reach a repair without a second
	 * path through the economy.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Barricade")
	TSubclassOf<UGameplayEffect> ScrapAwardEffect;

	/**
	 * The window opening: what blocks movement and what the interaction trace hits.
	 *
	 * The boards are decoration and carry no collision, so this has to be here whatever the count
	 * is — a stripped barricade with nothing left to trace against could not be repaired at all.
	 * It blocks pawns at every count; getting through a breached window is `TraverseWindow`'s job
	 * in 7.4, not a hole anything can walk into.
	 */
	/**
	 * The actor's origin, and it sits on the **ground**.
	 *
	 * Not in the middle of the wall, which is where a box-as-root put it and which meant every
	 * height in this class was quoted from an arbitrary line halfway up. A barricade is placed on
	 * a floor, its sill is a height off that floor, and an enemy lands on it: ground zero is the
	 * one origin all three agree on, and it is also where the window meshes are authored from.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	TObjectPtr<USceneComponent> Origin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	TObjectPtr<UBoxComponent> Frame;

	/**
	 * The wall the window is cut into, built from the layout asset's pieces.
	 *
	 * Solid to shots and to physics bodies, like the boards (the `RSBarricade` collision profile),
	 * but never to movement: `Frame` is still the one thing that decides who walks up to and over
	 * the window. It is also here so that a vault over a hundred-centimetre sill has a sill to
	 * vault over.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "RS|Barricade")
	TArray<TObjectPtr<UStaticMeshComponent>> FramePieces;

	/**
	 * Board meshes, in tear order. Board `i` is up while `i < BoardCount`.
	 *
	 * `Transient`, like the two arrays around it, and that is not an optimisation. These hold
	 * components the constructor makes; letting them serialise means a Blueprint saved when there
	 * were three slots still hands back three after the code says two, and the extra one points
	 * at a component nothing places any more. The components themselves are still saved, by name,
	 * exactly as the engine always saved them — it is only the bookkeeping that is rebuilt.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "RS|Barricade")
	TArray<TObjectPtr<UStaticMeshComponent>> Boards;

	/** Where enemies stand to tear boards. +X is the outside. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "RS|Barricade")
	TArray<TObjectPtr<USceneComponent>> AttackSlots;

#if WITH_EDITORONLY_DATA
	/**
	 * Editor only: one arrow per reach gap, coming in from outside with its tip on the layout's
	 * `ReachPoints` entry, which is where the reach's blow starts. For tuning the points by eye
	 * in the viewport; the layout asset refreshes every barricade using it when it is edited.
	 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "RS|Barricade")
	TArray<TObjectPtr<UArrowComponent>> ReachGapArrows;

	/** Editor only: one arrow per attack slot, pointing the way its enemy faces while it reaches. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "RS|Barricade")
	TArray<TObjectPtr<UArrowComponent>> ReachFacingArrows;
#endif

private:
	UPROPERTY(ReplicatedUsing = OnRep_BoardCount)
	int32 BoardCount = 6;

	/** Hands every free place to the queued enemy nearest it, until the places or the queue run out. */
	void HandFreeSlotsToQueue();

	/** Who holds each slot. Server only, and weak on purpose: a dead claimant frees its slot. */
	TArray<TWeakObjectPtr<AActor>> SlotHolders;

	/** Enemies waiting for a place, in no order: who gets one is decided by distance when it frees. Server only, weak. */
	TArray<TWeakObjectPtr<AActor>> QueuedClaimants;

	/** Who is climbing through, if anyone. Weak for the same reason: a body does not block a window. */
	TWeakObjectPtr<AActor> TransitHolder;
};
