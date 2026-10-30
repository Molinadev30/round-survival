// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_Vault.generated.h"

class ARSBarricade;
class UAnimMontage;

/**
 * Climbing in through a window whose boards are gone.
 *
 * The enemy does not find its way in — it was handed one at birth by the spawn point behind it,
 * so this ability never searches for a window, only crosses the one it already has. What it does
 * have to solve is that the traversal animation was authored over one particular obstacle and the
 * window is whatever the level says it is. Two motion warp targets answer that: `FrontLedge` puts
 * the plant on top of this sill, `BackLedge` puts the landing on this floor, and the montage is
 * stretched between them. Both come off the barricade's own `SillHeight`, so a window drawn at a
 * different height is crossed correctly without a second clip.
 *
 * **The frame is ignored for the duration, and only for movement.** `MoveIgnoreActorAdd` takes the
 * barricade out of the capsule's movement sweeps so root motion can carry the enemy through a
 * solid box; it touches no trace channel, so nothing about shooting, aiming or line of sight
 * changes while an enemy is climbing. The ignore is lifted whatever way the ability ends,
 * including a cancel, because an enemy left able to walk through walls is worse than one stuck.
 *
 * Crossing is exclusive: the barricade hands out one transit claim at a time, so a breached window
 * stays a bottleneck instead of becoming an open door. On the far side the ability clears the
 * enemy's entry barricade — it is inside now, the window is no longer its way in, and the State
 * Tree sends it back to the chase on the next tick without needing to be told separately.
 *
 * Server only and unpredicted, like everything else an enemy does.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_Vault : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Vault(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End of UGameplayAbility interface

	/** The window being crossed: the one the enemy came in through. */
	ARSBarricade* GetTargetBarricade() const;

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	/** Undoes everything `ActivateAbility` did to the pawn. Safe to call twice. */
	void StopCrossing(bool bArrivedInside);

	/**
	 * The climb. Its two motion warping notify states are what bend it onto this window, so the
	 * names below have to match the ones authored on the montage.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Vault")
	TObjectPtr<UAnimMontage> VaultMontage;

	/**
	 * The climb for an enemy that has lost a leg (`RS.State.Crawling`), with the same two warp
	 * windows. Empty, it climbs with `VaultMontage` like everyone else.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Vault")
	TObjectPtr<UAnimMontage> CrawlingVaultMontage;

	/** Warp target for the plant on the sill. Must match the montage's first notify state. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Vault")
	FName FrontLedgeTargetName = TEXT("FrontLedge");

	/** Warp target for the landing inside. Must match the montage's second notify state. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Vault")
	FName BackLedgeTargetName = TEXT("BackLedge");

	/** Playback rate, which is also how long an enemy spends defenceless on the sill. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Vault", meta = (ClampMin = 0.01))
	float MontageRate = 1.0f;

	/**
	 * How far back from the window's outside face the root is put at the plant.
	 *
	 * The barricade says where the sill is; where the root has to be for the hands to land on it is
	 * a property of the clip, so it lives here. Measured on this clip: with the root on the outside
	 * face the wrists came down on the inside edge and the fingers hung past it. 12 cm puts the
	 * wrists on the middle of the sill.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Vault", meta = (ForceUnits = cm))
	float FrontLedgeSetback = 12.0f;

	/**
	 * How far past the window's inner face the root is put down on landing.
	 *
	 * 162 cm is where this clip's own root lands, so the drop inside plays as authored. Shorter
	 * lands the enemy tight against the wall: the warp squeezes the far half of the climb, so the
	 * body comes down steeper. Never less than the capsule's radius — a body landing closer than
	 * that would be put down inside the wall — and the floor has to be clear this far in.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Vault", meta = (ClampMin = 0.0, ForceUnits = cm))
	float LandingDistance = 162.0f;

	/**
	 * The latest the montage may start, in seconds, however close the enemy is.
	 *
	 * The start is distance matched: see `FindDistanceMatchedStartTime`. This caps it before the
	 * push-off, so an enemy standing almost against the wall still plays the crouch that launches
	 * it rather than appearing on the sill already in the air.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Vault", meta = (ClampMin = 0.0, ForceUnits = s))
	float MaxMontageStartTime = 0.3f;

	/**
	 * Where in the montage to start so that the clip's own walk-up covers the distance left.
	 *
	 * The clip was authored with a run-up: its root travels 69 cm along the ground before it plants
	 * on the sill. An enemy is never exactly that far out — the navmesh stops it wherever its edge
	 * happens to fall — and starting the clip from the top every time hands the whole difference to
	 * the warp, which reads as a leap from too far or a shuffle on the spot. Starting at the frame
	 * whose remaining distance to the plant matches the enemy's own is distance matching: the feet
	 * leave the ground at the same spot against the wall wherever the enemy stopped, and the warp
	 * is left only the few centimetres of error.
	 *
	 * The distance is read off the montage's root motion rather than off an authored curve, so a
	 * different clip needs no extra setup — only its `FrontLedge` warp window.
	 */
	float FindDistanceMatchedStartTime(const UAnimMontage* Montage, float DistanceToPlant) const;

	/** End of the montage's warp window for `WarpTargetName`: where the clip plants. Negative if absent. */
	float FindWarpWindowEnd(const UAnimMontage* Montage, FName WarpTargetName) const;

	/** The climb this enemy's body can do: `CrawlingVaultMontage` for a crawler, `VaultMontage` otherwise. */
	UAnimMontage* GetVaultMontage() const;

private:
	/** The window we took a transit claim on, so the claim is given back to the right actor. */
	UPROPERTY()
	TWeakObjectPtr<ARSBarricade> CrossingBarricade;

	bool bCrossing = false;

	/** What the capsule did to other pawns before the climb, so the end of it can put that back. */
	ECollisionResponse PawnResponseBeforeCrossing = ECR_Block;
};
