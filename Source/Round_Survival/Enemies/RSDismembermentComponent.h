// Round Survival

#pragma once

#include "Components/ActorComponent.h"
#include "Enemies/RSEnemyTypes.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"

#include "RSDismembermentComponent.generated.h"

class URSDismembermentProfile;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FGameplayEffectSpec;
struct FRSDismemberedMeshSet;

/**
 * What has come off an enemy, replicated as one value.
 *
 * State, not an event: a client that starts receiving this enemy after an arm came off still has
 * to draw it without the arm, and it learns that from the mask whenever it arrives. What is new
 * to a client is whatever the mask holds that the last one did not.
 */
USTRUCT()
struct FRSDismemberState
{
	GENERATED_BODY()

	/** One bit per ERSBodyPart that has come off. */
	UPROPERTY()
	uint8 SeveredMask = 0;

	/**
	 * Which way the blow that took the last part was travelling, for the piece and the blood.
	 * Zero when nothing travelled: the debug command.
	 */
	UPROPERTY()
	FVector_NetQuantizeNormal LastCutDirection = FVector::ZeroVector;
};

/**
 * Takes an enemy apart.
 *
 * The server decides. Each damaging hit is charged to the part it landed on — found from the bone
 * in the hit, walking up the skeleton to the bone a part hangs from — and a part that has taken
 * its share of the enemy's health comes off. Coming off applies the part's effect, which is where
 * losing it changes how the enemy plays, and tells the enemy's ability system with
 * RS.Event.Dismembered. What has come off replicates as a mask; every machine draws the body from
 * that.
 *
 * **Whole until the first cut.** A body nothing has come off is the one mesh it always was. At the
 * first cut the character's mesh becomes the torso — still the one that animates, simulates and is
 * hit — and the limbs still on are worn on it as pieces that follow its pose. What has gone keeps
 * its physics bodies, without collision and nearly without mass: nothing can hit it, and the bones
 * the stump's skin follows stay on the body when it falls.
 *
 * The thresholds and effects are data, in the enemy definition's URSDismembermentProfile.
 */
UCLASS()
class ROUND_SURVIVAL_API URSDismembermentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URSDismembermentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The tag naming a part, as RS.Event.Dismembered carries it. */
	static FGameplayTag GetPartTag(ERSBodyPart Part);

	/**
	 * Charges a hit to the part it landed on, and takes the part off if that was enough. Bound to
	 * the owner's health set; does anything only on the server, for a drop in health an effect
	 * caused.
	 *
	 * Runs before the death does — the health set reports the change before it reports running
	 * out — so the hit that kills can still take a part, the head included.
	 */
	void HandleHealthChanged(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
		float Magnitude, float OldValue, float NewValue);

	/**
	 * Takes a part off. Server only; false when it was already off.
	 *
	 * Whatever decided it — a hit's damage, the debug command — goes through here, so the effect,
	 * the event and the replicated mask always come together.
	 */
	bool SeverPart(ERSBodyPart Part, AActor* Instigator = nullptr, const FVector& CutDirection = FVector::ZeroVector);

	/** Everything back on and nothing charged: a body leaving the pool is a new enemy. Server only. */
	void ResetForNewLife();

	/** On every machine: the mask replicates. */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	bool IsSevered(ERSBodyPart Part) const { return (State.SeveredMask & PartBit(Part)) != 0; }

	/** One bit per ERSBodyPart that has come off. On every machine. */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	uint8 GetSeveredMask() const { return State.SeveredMask; }

	/**
	 * What this enemy can lose and how close each part is, for `RS.Enemy.Debug`. Server only: the
	 * damage charged to each part is not replicated.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	FString GetDebugString() const;

protected:
	//~UActorComponent interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of UActorComponent interface

	UFUNCTION()
	void OnRep_State(const FRSDismemberState& PreviousState);

	/**
	 * Where every machine follows a change of the mask: the server as it makes it, a client as it
	 * arrives. Draws the body from the whole mask rather than from what changed, so a client that
	 * missed a step — a body pooled and reused between two updates — still ends up right.
	 */
	void HandleStateChanged(uint8 PreviousMask);

	/** The body drawn for the current mask: the torso for what is still on, the limbs still attached worn on it, what has gone unhittable. */
	void TakeApart(uint8 NewlySevered);

	/**
	 * The cut itself, as it happens: the piece thrown, the blood, the sound. Read from the body's
	 * pose before it is taken apart, so the piece leaves from where the part was.
	 */
	void PlayCut(ERSBodyPart Part);

	/**
	 * Whether this machine should show a cut now: it happened while this machine was watching —
	 * not a state it is catching up on, as a client does on first receiving an enemy — and the
	 * body was on its screen. A dedicated server shows nothing.
	 */
	bool ShouldPlayCut() const;

	/** Whole again: the one mesh, the pieces hidden, and the bodies of every part in the mask back. */
	void RestoreWholeBody(uint8 SeveredMask);

	/** The piece a limb is worn as, made the first time this body needs it and kept for its later lives. */
	USkeletalMeshComponent* FindOrAddPartComponent(ERSBodyPart Part, USkeletalMesh* PartMesh);

	/** Worn: registered and posed by the torso. Not worn: unregistered, so it costs nothing until it is worn again. */
	void SetPartWorn(USkeletalMeshComponent* Piece, bool bWorn);

	const URSDismembermentProfile* GetProfile() const;

	/** The pieces of the body this enemy wore when it was last whole. */
	const FRSDismemberedMeshSet* GetMeshSet() const;

	/**
	 * The part a bone belongs to, or MAX when none does: the bone itself or the nearest of its
	 * ancestors that a rule names. A walk of a handful of parents, per damaging hit, rather than
	 * a table built from one mesh: a table would have to be rebuilt whenever the body changes
	 * mesh, and the walk cannot go stale.
	 */
	ERSBodyPart FindPartForBone(FName BoneName) const;

	static uint8 PartBit(ERSBodyPart Part) { return static_cast<uint8>(1u << static_cast<uint8>(Part)); }

private:
	UPROPERTY(ReplicatedUsing = OnRep_State)
	FRSDismemberState State;

	/** Damage charged to each part this life. Server only; nothing but the outcome is sent. */
	float PartDamage[static_cast<int32>(ERSBodyPart::MAX)] = {};

	/**
	 * What the body was before the first cut, put back when it is whole again. Null while it is
	 * whole: the character's own mesh is then the answer.
	 */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> WholeMesh;

	/** The limbs worn on the torso, by part. Made on demand and kept with the body. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> PartComponents;
};
