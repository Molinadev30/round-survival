// Round Survival

#pragma once

#include "Enemies/RSEnemyTypes.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "RSDismembermentProfile.generated.h"

class UGameplayEffect;
class UNiagaraSystem;
class USkeletalMesh;
class USoundBase;

/**
 * When one part of an enemy comes off, and what losing it does.
 */
USTRUCT(BlueprintType)
struct FRSBodyPartRule
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Part")
	ERSBodyPart Part = ERSBodyPart::ArmLeft;

	/**
	 * The bone the part hangs from. A hit on it or on anything below it is a hit on the part, so
	 * the rule names one bone rather than listing every body the physics asset puts on the limb,
	 * and a body added to the hand later is already covered.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Part")
	FName RootBone;

	/**
	 * Damage the part has to take before it comes off, as a fraction of the enemy's MaxHealth.
	 *
	 * A fraction rather than hit points, so the round's health scaling carries the limbs with it:
	 * an arm takes the same share of a round-30 enemy as of a round-1 one. Zero means the first
	 * hit that qualifies takes it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Part", meta = (ClampMin = 0.0, UIMin = 0.0, UIMax = 1.0))
	float SeverThreshold = 0.25f;

	/**
	 * Only the hit that kills can take the part. The head: an enemy that keeps walking without
	 * one is a question the game would have to answer, and one that falls is not.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Part")
	bool bOnlyOnKillingBlow = false;

	/**
	 * Applied to the enemy when the part comes off, on the server: what the loss does to it, as
	 * modifiers and granted tags. Empty means the loss is only seen.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Part")
	TSubclassOf<UGameplayEffect> SeverEffect;

	// -- Presentation ----------------------------------------------------------------------
	// Played on each machine that sees the cut happen, never on one that only learns of it later.

	/** Where the part comes off, facing along the blow. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> CutEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Presentation")
	TObjectPtr<USoundBase> CutSound;

	/** On the stump, pointing out of it, for StumpEffectDuration. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> StumpEffect;

	/** How long the stump bleeds. Zero leaves it to the effect's own length. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation", meta = (ClampMin = 0.0, ForceUnits = s))
	float StumpEffectDuration = 2.5f;

	/** On the cut end of the piece, for as long as the piece lasts. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> PieceEffect;

	/** How fast the piece leaves along the blow, on top of the body's own velocity. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation", meta = (ClampMin = 0.0, ForceUnits = "cm/s"))
	float PieceLaunchSpeed = 250.0f;

	/** And upwards, so a piece cut level does not slide along the floor. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation", meta = (ClampMin = 0.0, ForceUnits = "cm/s"))
	float PieceLift = 150.0f;
};

/**
 * One body's pieces: the meshes a whole body is drawn with once something has come off it.
 *
 * Every piece is on the whole body's skeleton, so it can be worn on the torso following its pose.
 * The torso carries a cap at every socket a limb leaves, hidden inside the limb while the limb is
 * on.
 */
USTRUCT(BlueprintType)
struct FRSDismemberedMeshSet
{
	GENERATED_BODY()

	/** The whole body these pieces take apart: what the enemy wears while nothing has come off, and again for its next life. */
	UPROPERTY(EditDefaultsOnly, Category = "Meshes")
	TObjectPtr<USkeletalMesh> IntactMesh;

	/** The torso and head, without the limbs. */
	UPROPERTY(EditDefaultsOnly, Category = "Meshes")
	TObjectPtr<USkeletalMesh> TorsoWithHead;

	/** The torso alone, capped at the neck: what is left once the head has gone. */
	UPROPERTY(EditDefaultsOnly, Category = "Meshes")
	TObjectPtr<USkeletalMesh> TorsoWithoutHead;

	/**
	 * Each part as a piece of its own. A limb is worn on the torso while it is on. The head is
	 * already part of TorsoWithHead, so its piece is only ever needed once it is off.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Meshes")
	TMap<ERSBodyPart, TObjectPtr<USkeletalMesh>> PartMeshes;
};

/**
 * How an enemy type comes apart, as data.
 *
 * The bones are the skeleton's, the thresholds are what a designer tunes, and the meshes are the
 * pieces each body is cut into. `URSEnemyDefinition` points at one; an enemy with none never loses
 * anything.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSDismembermentProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The rule for a part, or null when this body cannot lose it. */
	const FRSBodyPartRule* FindRule(ERSBodyPart Part) const;

	/** The pieces of a whole body, or null when nothing here can take that body apart. */
	const FRSDismemberedMeshSet* FindMeshSet(const USkeletalMesh* IntactMesh) const;

	/** One per part; a second for the same part is never read. A part with no entry stays on. */
	UPROPERTY(EditDefaultsOnly, Category = "Parts", meta = (TitleProperty = "Part"))
	TArray<FRSBodyPartRule> Parts;

	/**
	 * One per body the enemy can wear. A body with no set still loses its parts — what is gone stops
	 * being hit — but is drawn whole.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Meshes", meta = (TitleProperty = "IntactMesh"))
	TArray<FRSDismemberedMeshSet> MeshSets;
};
