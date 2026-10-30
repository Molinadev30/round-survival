// Round Survival

#pragma once

#include "Engine/DataAsset.h"

#include "RSBarricadeWindowLayout.generated.h"

class UStaticMesh;

/** One piece of the window's structure: a mesh and where it sits. */
USTRUCT(BlueprintType)
struct FRSBarricadeFramePiece
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	FTransform Transform;
};

/** How the enemy in one attack slot reaches through its gap. */
USTRUCT(BlueprintType)
struct FRSReachPoint
{
	GENERATED_BODY()

	/**
	 * Where the blow comes from, in opening space: the middle of the slot's gap. The reach's damage
	 * sweep starts here and travels inward, and a player is in reach when near it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	FVector Location = FVector::ZeroVector;

	/**
	 * Degrees the enemy turns while reaching, on top of facing the middle of the window.
	 * Positive turns it to its right. The reach animation plays as authored, so this is what
	 * lines its arm up with the gap: tune it until the arm comes out through the hole.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade", meta = (ForceUnits = deg))
	float YawOffset = 0.0f;

	/**
	 * Reaches with the other arm: the reach plays mirrored. For the slot whose gap is on the wrong
	 * side of the body for the arm the animation uses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	bool bMirrored = false;
};

/**
 * What every window in the game is built out of, and where every board on it sits.
 *
 * **One asset, shared by every barricade, on purpose.** A player who learns which gaps a boarded
 * window leaves has learned it for all of them: the sightlines through a window are a constant of
 * the game rather than a property of the window they happen to be standing at. That is why this
 * is an asset the barricade points at and not a per-instance array — an array on the actor is an
 * invitation to nudge one board on one window and quietly break the rule.
 *
 * Everything here is authored in **opening space**: the origin is the middle of the sill's top
 * edge, `+X` is the outside face, `+Y` runs across the window and `+Z` climbs towards the lintel.
 * A board's `Z` is therefore its height above the sill in centimetres, and the whole layout
 * survives a change of `SillHeight` without being re-authored — the barricade lifts it onto
 * whatever sill it has. Scale is used as authored, so a board's length is part of the layout:
 * how far a plank reaches across the hole decides what can be seen through it.
 *
 * **Board index is tear order.** Board zero is the last one an enemy takes and the first a player
 * puts back, so the array runs from the boards that hold the window together to the ones that
 * come off first.
 */
UCLASS(BlueprintType)
class ROUND_SURVIVAL_API URSBarricadeWindowLayout : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The wall and its window: sill, lintel, jambs, panels. Fixed geometry, never hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	TArray<FRSBarricadeFramePiece> FramePieces;

	/** One entry per board. Entries past the barricade's board count go unused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	TArray<FTransform> Boards;

	/**
	 * The gaps an enemy reaches through, one per attack slot and in slot order: left, middle, right.
	 *
	 * Fixed, and chosen with every board in place — the three widest holes the full barricade
	 * leaves. They stay the same as boards come off, so each slot always reaches through the same
	 * hole: a player learns three danger points per window instead of a set that shifts with every
	 * board. Each entry also says how the enemy in that slot turns and which arm it uses, since the
	 * reach itself is one animation played as authored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RS|Barricade")
	TArray<FRSReachPoint> ReachPoints;

#if WITH_EDITOR
	/** Rebuilds every barricade in the editor world that uses this layout, so edits show at once. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
