// Round Survival

#pragma once

#include "GameFramework/CharacterMovementComponent.h"

#include "RSCharacterMovementComponent.generated.h"

/** The player's own movement modes, under MOVE_Custom. */
UENUM()
enum class ERSCustomMovementMode : uint8
{
	None,
	Slide,
};

/**
 * How much sprint the player has left. Kept apart from the move timers because, unlike them, it
 * lasts the whole match: a timer that drifts between the client and the server for half a second
 * is gone at the end of the slide, while a sprint that drifts stays drifted. So the server sends
 * its own with every correction, and a replay carries it on from there (see PrepMoveFor).
 */
struct FRSSprintStamina
{
	/** How much of the sprint is spent, in seconds of sprinting. Zero is a full sprint. */
	float SpentSeconds = 0.0f;

	/** What is left of the pause after the last sprinting move before the sprint starts coming back. */
	float RechargeDelaySeconds = 0.0f;

	/**
	 * The sprint ran out with the key down, or the key went down with nothing left. Holding on does
	 * not sprint again as it comes back: letting go does, and the next press sprints on what there is.
	 */
	bool bExhausted = false;
};

/**
 * The server's answer to a move, with its sprint added when the answer is a correction. Corrected
 * without it, a client that had spent its sprint differently would replay its moves on its own
 * figure, part ways with the server again at the same point and be corrected for ever.
 */
struct FRSCharacterMoveResponseDataContainer : public FCharacterMoveResponseDataContainer
{
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;

	FRSSprintStamina SprintStamina;
};

/**
 * The player's movement, with what the player is doing that changes how fast they move carried in
 * every saved move.
 *
 * **Why a flag in the move and not an effect on an attribute.** Movement is predicted: the owning
 * client moves first and the server replays the same moves to check them. Whatever changes the speed
 * has to change it on both at the same move, or the server replays a move at a different speed from
 * the one the client made and corrects it. A gameplay effect lands on the client and the server at
 * different times — the server's copy arrives a round trip later — so a speed it changes is wrong on
 * one of them for that long. A flag packed into the saved move reaches the server inside the move it
 * belongs to, and the server replays that move with it set. That is how the engine carries crouching
 * and jumping, and it is what Epic's own examples do for sprinting.
 *
 * `MoveSpeedMultiplier` on the combat set stays for what the server decides alone — going down — where
 * the client has nothing to predict.
 *
 * **The slide** is a movement mode of its own (ERSCustomMovementMode::Slide), entered from a sprint by the
 * crouch key. A mode rather than a flag because the server's corrections carry the movement mode: a
 * client corrected into or out of a slide is put back in the right one, and everybody else sees it in
 * the mode the engine already replicates. Inside it the floor, steps and ledges are the walking
 * physics', run as they are; only the velocity is the slide's — a fixed burst on entry that friction
 * eats away, with no input acceleration, and a strafe that bends it to one side. Its timers live in
 * every saved move, so a replayed move starts from the timers it was first made with.
 *
 * **The sprint runs out.** Every sprinting move spends it, second for second, up to SprintDuration
 * scaled by the handling set's SprintDurationMultiplier; after a pause without sprinting it comes back.
 * Spent, the move is no longer a sprint, so everything that reads IsSprinting — speed, the slide, the
 * animation, the replicated flag — follows with nothing of its own. Sliding and jumping spend none.
 * See FRSSprintStamina for why the server's figure travels with its corrections.
 */
UCLASS()
class ROUND_SURVIVAL_API URSCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	URSCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Set by the aim down sights ability on the machine that controls the pawn. The server takes it
	 * from the client's moves; it never sets it for a remote player itself.
	 */
	void SetWantsToAimDownSights(bool bInWantsToAimDownSights) { bWantsToAimDownSights = bInWantsToAimDownSights; }

	bool WantsToAimDownSights() const { return bWantsToAimDownSights; }

	/** Set by the sprint ability on the machine that controls the pawn, the same way. */
	void SetWantsToSprint(bool bInWantsToSprint) { bWantsToSprint = bInWantsToSprint; }

	bool WantsToSprint() const { return bWantsToSprint; }

	/**
	 * Whether this move is a sprint: the sprint is wanted and not spent, the pawn is on the ground and
	 * standing, and the input points within SprintMaxAngle of where the pawn faces. Holding the key while strafing,
	 * backing off or standing still is not a sprint. The input is part of every saved move, so the
	 * server reaches the same answer when it replays it. Animation and the HUD read this, not the tag.
	 */
	bool IsSprinting() const;

	/**
	 * Asks for a slide, from the crouch key pressed mid-sprint or in the air, and drops the ask when it
	 * comes up. A slide starts the first move the ask finds the player able to; one asked for in the air
	 * waits for the landing. Starting one uses the ask up: another needs another press. Set on the
	 * machine that controls the pawn; the server takes it from the moves.
	 */
	void SetWantsToSlide(bool bInWantsToSlide) { bWantsToSlide = bInWantsToSlide; }

	bool WantsToSlide() const { return bWantsToSlide; }

	/** Whether the pawn is in the slide mode. Replicated with the movement mode, so true on every machine. */
	bool IsSliding() const;

	/**
	 * The timers the moves carry. Each saved move keeps them as they stood when it was made, and a move
	 * that is replayed or combined starts from those, the way it starts from its own location and velocity.
	 */
	struct FMoveTimers
	{
		/** How long the current sprint has lasted. Zero while not sprinting. */
		float SprintSeconds = 0.0f;

		/** How long the current slide has lasted. */
		float SlideSeconds = 0.0f;

		/** What is left of the stand-up after a slide, when speed is held down and there is no sprint. */
		float SlideRecoverySeconds = 0.0f;

		/** The side a slide can be bent to: 0 until the first strafe picks one, then 1 for right, -1 for left. */
		int8 SlideSteerSide = 0;

		/** A slide was asked for in the air: it starts on landing. */
		bool bSlidePending = false;
	};

	const FMoveTimers& GetMoveTimers() const { return MoveTimers; }
	void SetMoveTimers(const FMoveTimers& InMoveTimers) { MoveTimers = InMoveTimers; }

	const FRSSprintStamina& GetSprintStamina() const { return SprintStamina; }
	void SetSprintStamina(const FRSSprintStamina& InSprintStamina) { SprintStamina = InSprintStamina; }

	/** How long a full sprint lasts: SprintDuration scaled by the multiplier. */
	float GetSprintDuration() const { return SprintDuration * SprintDurationMultiplier; }

	/** Seconds of sprint the player has left. */
	UFUNCTION(BlueprintPure, Category = "RS|Movement")
	float GetSprintSecondsLeft() const { return FMath::Max(0.0f, GetSprintDuration() - SprintStamina.SpentSeconds); }

	/** Whether the sprint ran out and the key has not been let go since. */
	UFUNCTION(BlueprintPure, Category = "RS|Movement")
	bool IsSprintExhausted() const { return SprintStamina.bExhausted; }

	/**
	 * URSHandlingSet::SprintDurationMultiplier, pushed in by the pawn when it changes. Kept here rather
	 * than read from the ability system each move: the moves run every frame and it changes once a
	 * purchase. Takes effect at once, both ways: a longer sprint gives back what it adds, a shorter
	 * one can leave the player already spent.
	 */
	void SetSprintDurationMultiplier(float InMultiplier) { SprintDurationMultiplier = InMultiplier; }

	//~UCharacterMovementComponent interface
	virtual float GetMaxSpeed() const override;
	virtual bool IsMovingOnGround() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual bool CanWalkOffLedges() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;

protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	//~End of UCharacterMovementComponent interface

	/**
	 * Ground acceleration from walking speed up to sprinting speed. Below walking speed the pawn
	 * accelerates as it always does; this is the slower build from a walk to a full sprint.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement", meta = (ForceUnits = "cm/s^2", ClampMin = 1.0))
	float SprintAcceleration = 1150.0f;

	/** Speed the slide starts at, in the direction the pawn was moving. Set, not added: every slide starts the same. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = "cm/s", ClampMin = 0.0))
	float SlideSpeed = 1420.0f;

	/** How long a slide lasts, unless something ends it first. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = s, ClampMin = 0.0))
	float SlideDuration = 0.5f;

	/** The fraction of its speed a slide loses per second on level ground. Nothing else slows it. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ClampMin = 0.0))
	float SlideFriction = 1.05f;

	/** Friction scale sliding down a slope, and up one. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ClampMin = 0.0))
	float SlideDownhillFrictionScale = 0.007f;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ClampMin = 0.0))
	float SlideUphillFrictionScale = 3.0f;

	/**
	 * How far the slide's direction has to point into or out of the floor's normal to count as a slope:
	 * the dot product of the two, about 0.2 for a slope of 11 degrees taken straight on.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float SlideSlopeThreshold = 0.2f;

	/** A slide that slows below this ends there. On level ground it never does; uphill it can. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = "cm/s", ClampMin = 0.0))
	float SlideMinSpeed = 480.0f;

	/** Slowest the pawn can be going to start a slide. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = "cm/s", ClampMin = 0.0))
	float SlideMinStartSpeed = 320.0f;

	/** How long the pawn has to have been sprinting to slide. A slide asked for in the air skips it. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = s, ClampMin = 0.0))
	float SlideMinSprintSeconds = 0.2f;

	/** How far off the pawn's facing the input may point to start a slide. Tighter than the sprint's. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = deg, ClampMin = 0.0, ClampMax = 90.0))
	float SlideMaxInputAngle = 37.0f;

	/** Room the slide needs ahead of the capsule, at crouching height, to start. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = cm, ClampMin = 0.0))
	float SlideClearance = 64.0f;

	/** How fast a full strafe bends the slide, in degrees a second. Its speed is untouched. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = "deg/s", ClampMin = 0.0))
	float SlideSteerRate = 35.0f;

	/** How hard the strafe has to be, as a fraction of a full one, to pick the side the slide bends to. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float SlideSteerInputThreshold = 0.25f;

	/**
	 * Standing up out of a slide: for this long the pawn cannot sprint, its speed is held to
	 * SlideRecoveryMaxSpeed, and what it carries over that is shed at SlideRecoveryFriction.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = s, ClampMin = 0.0))
	float SlideRecoveryDuration = 0.33f;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ForceUnits = "cm/s", ClampMin = 0.0))
	float SlideRecoveryMaxSpeed = 255.0f;

	/** The fraction of its speed over the limit the pawn loses per second while standing up. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Slide", meta = (ClampMin = 0.0))
	float SlideRecoveryFriction = 5.5f;

	/** Ground speed while aiming down sights, as a fraction of the speed otherwise. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement", meta = (ForceUnits = x, ClampMin = 0.0, ClampMax = 1.0))
	float AimDownSightsSpeedMultiplier = 0.6f;

	/** Ground speed while sprinting, as a multiple of the speed otherwise. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement", meta = (ForceUnits = x, ClampMin = 1.0))
	float SprintSpeedMultiplier = 1.5f;

	/** How far off the pawn's facing the input may point and still sprint. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement", meta = (ForceUnits = deg, ClampMin = 0.0, ClampMax = 90.0))
	float SprintMaxAngle = 50.0f;

	/** How long a full sprint lasts, before the handling set's multiplier. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Sprint", meta = (ForceUnits = s, ClampMin = 0.1))
	float SprintDuration = 4.0f;

	/** How long after the last sprinting move the sprint starts coming back. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Sprint", meta = (ForceUnits = s, ClampMin = 0.0))
	float SprintRechargeDelay = 1.0f;

	/** Seconds of sprint given back per second once it is coming back. Two refills an empty 4 s sprint in 2 s. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Movement|Sprint", meta = (ClampMin = 0.0))
	float SprintRechargeRate = 2.0f;

private:
	/** Spends the sprint on a sprinting move, brings it back after the pause otherwise, and latches the run-out. */
	void UpdateSprintStamina(float DeltaSeconds);

	/** The input as a fraction of a full push, from the acceleration it was scaled into. */
	FVector GetInputVector() const;

	/** Whether a slide can start this move. One asked for in the air needs the sprint key, not a sprint. */
	bool CanStartSlide(bool bFromAir) const;

	/** Whether a crouching capsule fits SlideClearance ahead of the pawn along Direction. */
	bool HasSlideClearance(const FVector& Direction) const;

	void StartSlide();

	/** Leaves the slide, standing up into the recovery or staying crouched. */
	void EndSlide(bool bStand);

	/** The slide's velocity for one step: bent by the strafe, slowed by friction. No input acceleration. */
	void CalcSlideVelocity(float DeltaTime);

	bool bWantsToAimDownSights = false;
	bool bWantsToSprint = false;
	bool bWantsToSlide = false;

	FMoveTimers MoveTimers;
	FRSSprintStamina SprintStamina;
	float SprintDurationMultiplier = 1.0f;

	/** The storage the engine fills and sends the server's answers from, ours so it carries the sprint. */
	FRSCharacterMoveResponseDataContainer RSMoveResponseDataContainer;
};
