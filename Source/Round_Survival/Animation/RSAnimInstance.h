// Round Survival

#pragma once

#include "Animation/AnimInstance.h"

#include "RSAnimInstance.generated.h"

class ACharacter;
class UAnimSequenceBase;
class UCharacterMovementComponent;

/**
 * Base animation instance for the player's viewmodel and body.
 *
 * It exists so that animation Blueprints contain graphs and nothing else: every value a state
 * machine or blend space needs is computed here, in C++, and the Blueprint only reads it. A
 * reviewer can see what drives the animation without opening a graph.
 *
 * Movement values are computed on the worker thread. Ability system state is not — gameplay tag
 * queries are not thread safe — so those bools are refreshed on the game thread instead.
 *
 * **Left-hand grip.** The drawn weapon hands this the poses for its grip (see
 * URSWeaponInstance::GetLeftHandGrip); the graph layers them over the left arm with
 * LeftHandGripAlpha. The montages that take the hand away — reload, equip — say when, with
 * URSAnimNotifyState_LeftHandGrip windows: the alpha falls to zero inside a window, so the
 * montage has the arm, and rises again as it closes, while the montage is still playing.
 */
UCLASS()
class ROUND_SURVIVAL_API URSAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	URSAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * The poses that put the left hand on the drawn weapon's grip; null for the handguard, which is
	 * the animations' own hold and needs no layer. A null pose fades the old one out rather than
	 * cutting it.
	 */
	void SetLeftHandGripPoses(UAnimSequenceBase* IdlePose, UAnimSequenceBase* AimPose);

	/** A montage window takes the hand off the grip. Windows nest: the grip returns when the last one closes. */
	void ReleaseLeftHandGrip(float BlendOutSpeed);

	/** A montage window closes and gives the hand back. */
	void ReturnLeftHandGrip(float BlendReturnSpeed);

protected:
	//~UAnimInstance interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	//~End of UAnimInstance interface

	/** Horizontal speed in cm/s. Drives the locomotion blend space. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	float Speed = 0.0f;

	/** Movement direction relative to where the pawn is facing, -180 to 180. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	float Direction = 0.0f;

	/**
	 * Velocity along the pawn's own axes, as a percentage of max walk speed, -100 to 100.
	 *
	 * This is what the viewmodel blend space wants: its axes are Horizontal and Vertical over
	 * that range, not a speed and an angle. Feeding it Speed and Direction would put the arms in
	 * the wrong cell of the grid at every angle except straight ahead.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	float MoveForwardPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	float MoveRightPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	bool bIsCrouched = false;

	/**
	 * In the slide movement mode. The mode replicates with the movement, so this is true on every
	 * machine's copy of the player, and it is checked before crouching: a slide is crouched too.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	bool bIsSliding = false;

	/** Up is positive. In the air, rising is the jump and falling is the fall. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	float VerticalSpeed = 0.0f;

	/** How far into the sprint pose, 0 to 1, eased over SprintBlendSeconds. From ARSPlayerCharacter::IsSprinting. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	float SprintAlpha = 0.0f;

	/** How far into the crouched pose, 0 to 1, eased over CrouchBlendSeconds. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	float CrouchAlpha = 0.0f;

	/** Time to go fully into or out of the sprint pose. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Locomotion", meta = (ForceUnits = s, ClampMin = 0.0))
	float SprintBlendSeconds = 0.2f;

	/** Time to go fully into or out of the crouched pose. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Locomotion", meta = (ForceUnits = s, ClampMin = 0.0))
	float CrouchBlendSeconds = 0.2f;

	/**
	 * Where the player looks, up or down, in degrees: positive is up. The same on every machine, so
	 * everyone sees the body aim where its owner does. Drives the procedural aim offset.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Aim")
	float AimPitch = 0.0f;

	/**
	 * The pitch the torso and the weapon take: AimPitch, eased into SprintCarryPitch while sprinting.
	 * The head is not on it; it always looks along AimPitch.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Aim")
	float BodyAimPitch = 0.0f;

	/**
	 * Where a sprinting player carries the weapon, as a pitch: below level lowers the rifle and,
	 * through the aim offset's bend, leans the spine into the run.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Aim", meta = (ForceUnits = deg))
	float SprintCarryPitch = -35.0f;

	/** How fast a remote copy's pitch follows the replicated one, as an interpolation speed. 0 snaps. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Aim", meta = (ClampMin = 0.0))
	float AimPitchInterpSpeed = 15.0f;

	/** True while the ADS ability holds `RS.State.ADS`. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	bool bIsAiming = false;

	/**
	 * How far into the sights, 0 at the hip to 1 fully aimed: the pawn's own alpha, the one the
	 * camera zooms by, so the arms come up exactly as the view narrows. Graphs blend the aimed
	 * poses in by it rather than switching on bIsAiming.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	float AimDownSightsAlpha = 0.0f;

	/** True while the reload ability is running, from `RS.State.Reloading`. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	bool bIsReloading = false;

	/** True while a weapon is being holstered or drawn, from `RS.State.SwitchingWeapon`. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	bool bIsSwitchingWeapon = false;

	/** The left-hand grip pose while not aiming. Only meaningful while LeftHandGripAlpha is above zero. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	TObjectPtr<UAnimSequenceBase> LeftHandGripIdlePose;

	/** The same while aiming down sights; the idle pose when the weapon has no aim pose for it. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	TObjectPtr<UAnimSequenceBase> LeftHandGripAimPose;

	/** How much of the left arm the grip pose owns, 0 to 1. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	float LeftHandGripAlpha = 0.0f;

	/**
	 * How fast LeftHandGripAlpha follows its target, as an interpolation speed, until a montage
	 * window sets its own.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ClampMin = 0.0))
	float LeftHandGripBlendSpeed = 15.0f;

private:
	/** Moves the grip alpha towards its target. */
	void UpdateLeftHandGrip(float DeltaSeconds);

	/** Whether the current grip has a pose at all; false on the handguard. */
	bool bHasLeftHandGripPose = false;

	/** Montage windows currently holding the hand off the grip. */
	int32 LeftHandGripReleaseCount = 0;

	/** The speed of the last release or return; LeftHandGripBlendSpeed until a window sets one. */
	TOptional<float> LeftHandGripWindowBlendSpeed;

	/** Cached on init so the worker thread never walks the actor hierarchy looking for them. */
	TWeakObjectPtr<ACharacter> OwningCharacter;
	TWeakObjectPtr<UCharacterMovementComponent> OwningMovement;
};
