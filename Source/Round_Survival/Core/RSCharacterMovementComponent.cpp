// Round Survival

#include "Core/RSCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/RSPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	/** The engine's first free compressed flags. Each player-driven change to the movement takes one. */
	constexpr uint8 FLAG_AimDownSights = FSavedMove_Character::FLAG_Custom_0;
	constexpr uint8 FLAG_Sprint = FSavedMove_Character::FLAG_Custom_1;
	constexpr uint8 FLAG_Slide = FSavedMove_Character::FLAG_Custom_2;

	/** A saved move that remembers the player's flags and timers, so a replayed move is replayed with them. */
	class FRSSavedMove : public FSavedMove_Character
	{
	public:
		virtual void Clear() override
		{
			FSavedMove_Character::Clear();
			bSavedWantsToAimDownSights = false;
			bSavedWantsToSprint = false;
			bSavedWantsToSlide = false;
			SavedTimers = URSCharacterMovementComponent::FMoveTimers();
			SavedSprintStamina = FRSSprintStamina();
		}

		virtual uint8 GetCompressedFlags() const override
		{
			uint8 Flags = FSavedMove_Character::GetCompressedFlags();
			if (bSavedWantsToAimDownSights)
			{
				Flags |= FLAG_AimDownSights;
			}
			if (bSavedWantsToSprint)
			{
				Flags |= FLAG_Sprint;
			}
			if (bSavedWantsToSlide)
			{
				Flags |= FLAG_Slide;
			}
			return Flags;
		}

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
		{
			// Two moves with different flags are not one move.
			const FRSSavedMove* Other = static_cast<const FRSSavedMove*>(NewMove.Get());
			if (bSavedWantsToAimDownSights != Other->bSavedWantsToAimDownSights || bSavedWantsToSprint != Other->bSavedWantsToSprint
				|| bSavedWantsToSlide != Other->bSavedWantsToSlide)
			{
				return false;
			}
			return FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);
		}

		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
		{
			FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

			if (const URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(C->GetCharacterMovement()))
			{
				bSavedWantsToAimDownSights = Movement->WantsToAimDownSights();
				bSavedWantsToSprint = Movement->WantsToSprint();
				bSavedWantsToSlide = Movement->WantsToSlide();
			}
		}

		virtual void SetInitialPosition(ACharacter* C) override
		{
			FSavedMove_Character::SetInitialPosition(C);

			// The timers as the move starts, next to where it starts. Also called again after a combine,
			// once the timers are rolled back to the start of the older move.
			if (const URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(C->GetCharacterMovement()))
			{
				SavedTimers = Movement->GetMoveTimers();
				SavedSprintStamina = Movement->GetSprintStamina();
			}
		}

		virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override
		{
			FSavedMove_Character::CombineWith(OldMove, InCharacter, PC, OldStartLocation);

			// The engine puts the pawn back where the older move started, to play both again as one.
			// The timers and the sprint go back with it.
			if (URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(InCharacter->GetCharacterMovement()))
			{
				const FRSSavedMove* Old = static_cast<const FRSSavedMove*>(OldMove);
				Movement->SetMoveTimers(Old->SavedTimers);
				Movement->SetSprintStamina(Old->SavedSprintStamina);
			}
		}

		virtual void PrepMoveFor(ACharacter* C) override
		{
			FSavedMove_Character::PrepMoveFor(C);

			// A correction replays the client's own moves: each one with the flags and timers it was made with.
			// Not the sprint: the correction brought the server's, and the replay spends it from there
			// on. Putting back the client's own would undo exactly what the correction was sent to fix.
			if (URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(C->GetCharacterMovement()))
			{
				Movement->SetWantsToAimDownSights(bSavedWantsToAimDownSights);
				Movement->SetWantsToSprint(bSavedWantsToSprint);
				Movement->SetWantsToSlide(bSavedWantsToSlide);
				Movement->SetMoveTimers(SavedTimers);
			}
		}

	private:
		bool bSavedWantsToAimDownSights = false;
		bool bSavedWantsToSprint = false;
		bool bSavedWantsToSlide = false;
		URSCharacterMovementComponent::FMoveTimers SavedTimers;
		FRSSprintStamina SavedSprintStamina;
	};

	class FRSNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
	{
	public:
		explicit FRSNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement)
			: FNetworkPredictionData_Client_Character(ClientMovement)
		{
		}

		virtual FSavedMovePtr AllocateNewMove() override
		{
			return FSavedMovePtr(new FRSSavedMove());
		}
	};
}

void FRSCharacterMoveResponseDataContainer::ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	FCharacterMoveResponseDataContainer::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	// Filled once the server has run the moves this answers, as the location it sends is.
	if (const URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(&CharacterMovement))
	{
		SprintStamina = Movement->GetSprintStamina();
	}
}

bool FRSCharacterMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap)
{
	if (!FCharacterMoveResponseDataContainer::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}

	// Corrections only. A good move is acknowledged with a timestamp and nothing else, and adding
	// nine bytes to every acknowledgement would buy nothing: a client the server agrees with is right.
	if (IsCorrection())
	{
		Ar << SprintStamina.SpentSeconds;
		Ar << SprintStamina.RechargeDelaySeconds;

		uint8 bExhausted = SprintStamina.bExhausted ? 1 : 0;
		Ar.SerializeBits(&bExhausted, 1);
		SprintStamina.bExhausted = bExhausted != 0;
	}

	return !Ar.IsError();
}

URSCharacterMovementComponent::URSCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMoveResponseDataContainer(RSMoveResponseDataContainer);
}

bool URSCharacterMovementComponent::IsSliding() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(ERSCustomMovementMode::Slide) && UpdatedComponent;
}

bool URSCharacterMovementComponent::IsMovingOnGround() const
{
	// A slide is on the ground: it crouches, jumps and follows the floor as walking does.
	return Super::IsMovingOnGround() || IsSliding();
}

float URSCharacterMovementComponent::GetMaxSpeed() const
{
	float MaxSpeed = Super::GetMaxSpeed();

	// On the ground only: aiming does not slow a fall or a jump already in the air. The abilities never
	// let both flags be up at once; if they were, the slower one would win.
	if (bWantsToAimDownSights && IsMovingOnGround())
	{
		MaxSpeed *= AimDownSightsSpeedMultiplier;
	}
	else if (IsSprinting())
	{
		MaxSpeed *= SprintSpeedMultiplier;
	}

	// Standing up out of a slide holds everything down, aiming included.
	if (MoveTimers.SlideRecoverySeconds > 0.0f && IsMovingOnGround())
	{
		MaxSpeed = FMath::Min(MaxSpeed, SlideRecoveryMaxSpeed);
	}

	return MaxSpeed;
}

bool URSCharacterMovementComponent::CanCrouchInCurrentState() const
{
	// On the ground only — a slide counts. The engine also allows it while falling; here nobody
	// crouches in the air.
	return Super::CanCrouchInCurrentState() && IsMovingOnGround();
}

bool URSCharacterMovementComponent::CanWalkOffLedges() const
{
	// A crouching walk stops at an edge; a slide runs off it and falls, which ends it.
	return IsSliding() || Super::CanWalkOffLedges();
}

bool URSCharacterMovementComponent::CanAttemptJump() const
{
	// The engine refuses a jump while the crouch is wanted, and a slide wants it the whole way.
	return IsSliding() ? IsJumpAllowed() : Super::CanAttemptJump();
}

bool URSCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	const bool bWasSliding = IsSliding();
	if (!Super::DoJump(bReplayingMoves, DeltaTime))
	{
		return false;
	}

	// Jumping out of a slide keeps its direction but not its burst: the jump carries a sprint's speed at most.
	if (bWasSliding)
	{
		const float MaxJumpSpeed = MaxWalkSpeed * SprintSpeedMultiplier;
		const FVector Horizontal = FVector(Velocity.X, Velocity.Y, 0.0f).GetClampedToMaxSize(MaxJumpSpeed);
		Velocity.X = Horizontal.X;
		Velocity.Y = Horizontal.Y;
	}

	return true;
}

void URSCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsSliding())
	{
		CalcSlideVelocity(DeltaTime);
		return;
	}

	// Standing up out of a slide: the speed over the limit goes in proportion to itself, along the way
	// the pawn was already going, rather than the instant braking the engine applies over the limit.
	if (MoveTimers.SlideRecoverySeconds > 0.0f && IsMovingOnGround())
	{
		const float Speed = Velocity.Size2D();
		const float MaxSpeed = GetMaxSpeed();
		if (Speed > MaxSpeed)
		{
			Velocity *= FMath::Max(MaxSpeed / Speed, 1.0f - SlideRecoveryFriction * DeltaTime);
			return;
		}
	}

	// Past walking speed a sprint builds more slowly; below it, a sprint starts like a walk. Applied
	// here, to the velocity, and not through GetMaxAcceleration: the acceleration the client sends is
	// clamped by the server's own maximum, and a maximum that turns on the speed would clamp it
	// differently whenever the two sit either side of walking speed — a correction every time.
	if (IsSprinting() && Velocity.SizeSquared2D() >= FMath::Square(Super::GetMaxSpeed()))
	{
		const FVector InputAcceleration = Acceleration;
		Acceleration = Acceleration.GetClampedToMaxSize(SprintAcceleration);
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		Acceleration = InputAcceleration;
		return;
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void URSCharacterMovementComponent::CalcSlideVelocity(float DeltaTime)
{
	const FVector Direction = Velocity.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	// The strafe across the slide's own direction, not the view's: looking around does not steer.
	// The first strafe hard enough picks the side, and the slide only ever bends that way.
	const FVector SlideRight = FVector::CrossProduct(FVector::UpVector, Direction);
	const float Strafe = FVector::DotProduct(GetInputVector(), SlideRight);
	if (MoveTimers.SlideSteerSide == 0 && FMath::Abs(Strafe) >= SlideSteerInputThreshold)
	{
		MoveTimers.SlideSteerSide = Strafe > 0.0f ? 1 : -1;
	}
	if (MoveTimers.SlideSteerSide != 0 && Strafe * MoveTimers.SlideSteerSide > 0.0f)
	{
		Velocity = Velocity.RotateAngleAxis(SlideSteerRate * Strafe * DeltaTime, FVector::UpVector);
	}

	// Friction in proportion to the speed, so the slide slows hard at first and gently later. A slope
	// taken downhill barely slows it; uphill slows it three times as hard.
	float FrictionScale = 1.0f;
	if (CurrentFloor.IsWalkableFloor())
	{
		const float Slope = FVector::DotProduct(Direction, CurrentFloor.HitResult.ImpactNormal);
		if (Slope > SlideSlopeThreshold)
		{
			FrictionScale = SlideDownhillFrictionScale;
		}
		else if (Slope < -SlideSlopeThreshold)
		{
			FrictionScale = SlideUphillFrictionScale;
		}
	}

	Velocity *= FMath::Max(0.0f, 1.0f - SlideFriction * FrictionScale * DeltaTime);
}

void URSCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Leaving the ground crouched — launched, or off a ledge — stands the player up and drops the
	// wish too, so they land standing rather than crouching again on touchdown. The wish travels in
	// every saved move, so the server, replaying the same move, drops it at the same point.
	if (bWantsToCrouch && IsFalling() && CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		bWantsToCrouch = false;
	}

	UpdateSprintStamina(DeltaSeconds);

	MoveTimers.SprintSeconds = IsSprinting() ? MoveTimers.SprintSeconds + DeltaSeconds : 0.0f;
	MoveTimers.SlideRecoverySeconds = FMath::Max(0.0f, MoveTimers.SlideRecoverySeconds - DeltaSeconds);

	if (IsSliding())
	{
		MoveTimers.SlideSeconds += DeltaSeconds;

		if (!bWantsToCrouch || bWantsToAimDownSights)
		{
			// The crouch key again, or the sights: the slide is cancelled and the player stands.
			EndSlide(/*bStand=*/ true);
		}
		else if (MoveTimers.SlideSeconds >= SlideDuration || Velocity.Size2D() < SlideMinSpeed)
		{
			// Run out. Still sprinting and still pushing, the player stands up into the sprint;
			// otherwise they stay down.
			EndSlide(/*bStand=*/ bWantsToSprint && !GetInputVector().IsNearlyZero());
		}
	}
	else if (IsFalling())
	{
		if (bWantsToSlide)
		{
			MoveTimers.bSlidePending = true;
			bWantsToSlide = false;
		}
	}
	else if (IsMovingOnGround())
	{
		const bool bFromAir = MoveTimers.bSlidePending;
		MoveTimers.bSlidePending = false;

		if ((bWantsToSlide || bFromAir) && CanStartSlide(bFromAir))
		{
			StartSlide();
		}
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void URSCharacterMovementComponent::UpdateSprintStamina(float DeltaSeconds)
{
	const float Duration = GetSprintDuration();

	// Letting go is the only way out of a run-out. The ability lowers the flag on release.
	if (!bWantsToSprint)
	{
		SprintStamina.bExhausted = false;
	}

	// Whether this move sprints is decided before it is spent, so the move that empties it still
	// sprints and the next one does not.
	if (IsSprinting())
	{
		SprintStamina.SpentSeconds = FMath::Min(SprintStamina.SpentSeconds + DeltaSeconds, Duration);
		SprintStamina.RechargeDelaySeconds = SprintRechargeDelay;
		SprintStamina.bExhausted = SprintStamina.SpentSeconds >= Duration;
		return;
	}

	// The key down with nothing left — held through the run-out, or pressed again before any came
	// back — waits for a fresh press, not for the first sliver to come back.
	if (bWantsToSprint && SprintStamina.SpentSeconds >= Duration)
	{
		SprintStamina.bExhausted = true;
	}

	if (SprintStamina.RechargeDelaySeconds > 0.0f)
	{
		SprintStamina.RechargeDelaySeconds = FMath::Max(0.0f, SprintStamina.RechargeDelaySeconds - DeltaSeconds);
	}
	else
	{
		SprintStamina.SpentSeconds = FMath::Max(0.0f, SprintStamina.SpentSeconds - SprintRechargeRate * DeltaSeconds);
	}
}

bool URSCharacterMovementComponent::CanStartSlide(bool bFromAir) const
{
	if (bWantsToAimDownSights || !CanCrouchInCurrentState())
	{
		return false;
	}

	// From the air the sprint key is enough: there is no sprint in the air to have lasted.
	const bool bSprintReady = bFromAir ? bWantsToSprint : (IsSprinting() && MoveTimers.SprintSeconds >= SlideMinSprintSeconds);
	if (!bSprintReady || Velocity.Size2D() < SlideMinStartSpeed)
	{
		return false;
	}

	const FVector InputDirection = GetInputVector().GetSafeNormal2D();
	const FVector Facing = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	if (FVector::DotProduct(InputDirection, Facing) < FMath::Cos(FMath::DegreesToRadians(SlideMaxInputAngle)))
	{
		return false;
	}

	return HasSlideClearance(Velocity.GetSafeNormal2D());
}

bool URSCharacterMovementComponent::HasSlideClearance(const FVector& Direction) const
{
	const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return false;
	}

	// At the middle of a crouching capsule standing on the same floor: low enough to find what a crouch
	// would hit, high enough to step over what walking steps over.
	const float CrouchedCentreHeight = GetCrouchedHalfHeight() - Capsule->GetScaledCapsuleHalfHeight();
	const FVector Start = UpdatedComponent->GetComponentLocation() + FVector(0.0f, 0.0f, CrouchedCentreHeight);
	const FVector End = Start + Direction * (Capsule->GetScaledCapsuleRadius() + SlideClearance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RSSlideClearance), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(Params, ResponseParams);

	return !GetWorld()->LineTraceTestByChannel(Start, End, UpdatedComponent->GetCollisionObjectType(), Params, ResponseParams);
}

void URSCharacterMovementComponent::StartSlide()
{
	bWantsToSlide = false;
	MoveTimers.SlideSeconds = 0.0f;
	MoveTimers.SlideSteerSide = 0;

	Velocity = Velocity.GetSafeNormal2D() * SlideSpeed;

	// The mode first, then the crouch: the pawn goes from standing straight to sliding, never through
	// crouching, which would move the camera at the crouch's rate. The slide keeps the feet on the floor
	// as a crouch does (OnMovementModeChanged).
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ERSCustomMovementMode::Slide));

	bWantsToCrouch = true;
	Crouch(/*bClientSimulation=*/ false);
}

void URSCharacterMovementComponent::EndSlide(bool bStand)
{
	if (bStand)
	{
		// Up while still in the slide, which keeps the feet on the floor. With no room above, the
		// pawn stays down, and the engine stands it up once there is.
		bWantsToCrouch = false;
		UnCrouch(/*bClientSimulation=*/ false);

		if (!IsCrouching())
		{
			MoveTimers.SlideRecoverySeconds = SlideRecoveryDuration;
		}
	}

	SetMovementMode(MOVE_Walking);
}

void URSCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// The engine treats every mode but walking as off the ground: it forgets the floor, and a crouch
	// would no longer keep the feet down. A slide is on the ground, so both come back.
	if (IsSliding())
	{
		bCrouchMaintainsBaseLocation = true;
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	}
}

void URSCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	// The walking physics, floor, steps and ledges included; CalcVelocity is where the slide differs.
	// Off a ledge it falls, which ends the slide.
	if (IsSliding())
	{
		// Everybody else's copy only carries on along the velocity the server sent, as a walking copy
		// does there; the slide's own physics run where the moves are made and checked. The engine
		// sends a copy in a custom mode here too, and the walking physics run on it dropped it out of
		// the mode between updates, each update putting it back.
		if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
		{
			const FVector Delta = Velocity * DeltaTime;
			FHitResult Hit(1.0f);
			SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
			if (Hit.IsValidBlockingHit())
			{
				SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
			}
			return;
		}

		PhysWalking(DeltaTime, Iterations);
		return;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

void URSCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	// After every move the server runs, its answer goes on the pawn for everybody else to see. Only
	// a change is sent.
	if (ARSPlayerCharacter* Player = Cast<ARSPlayerCharacter>(CharacterOwner); Player && Player->HasAuthority())
	{
		Player->SetServerSprinting(IsSprinting());
	}
}

bool URSCharacterMovementComponent::IsSprinting() const
{
	if (!bWantsToSprint || !IsMovingOnGround() || IsCrouching() || !UpdatedComponent || MoveTimers.SlideRecoverySeconds > 0.0f)
	{
		return false;
	}

	// Spent, or run out with the key still down. The first catches a multiplier that has just shrunk
	// the sprint under what is already spent, before the next move latches it.
	if (SprintStamina.bExhausted || SprintStamina.SpentSeconds >= GetSprintDuration())
	{
		return false;
	}

	// The input, not the velocity: a sprint starts on the press, before the pawn has sped up, and
	// stops the moment the input turns away, not once momentum has.
	const FVector InputDirection = GetCurrentAcceleration().GetSafeNormal2D();
	if (InputDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector Facing = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(InputDirection, Facing) >= FMath::Cos(FMath::DegreesToRadians(SprintMaxAngle));
}

FVector URSCharacterMovementComponent::GetInputVector() const
{
	const float CurrentMaxAcceleration = GetMaxAcceleration();
	return CurrentMaxAcceleration > 0.0f ? Acceleration / CurrentMaxAcceleration : FVector::ZeroVector;
}

void URSCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// On the server, for a remote player: the client's flags as they were when it made this move.
	bWantsToAimDownSights = (Flags & FLAG_AimDownSights) != 0;
	bWantsToSprint = (Flags & FLAG_Sprint) != 0;
	bWantsToSlide = (Flags & FLAG_Slide) != 0;
}

FNetworkPredictionData_Client* URSCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		URSCharacterMovementComponent* MutableThis = const_cast<URSCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FRSNetworkPredictionData_Client(*this);
	}

	return ClientPredictionData;
}

void URSCharacterMovementComponent::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	Super::ClientHandleMoveResponse(MoveResponse);

	// Taken only from a correction the engine acted on — the one that acknowledged the move it
	// answers and asked for a replay. One for a move already acknowledged is dropped, and its sprint
	// is as old as its location. The replay that follows spends it from here.
	const FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (MoveResponse.IsCorrection() && ClientData && ClientData->bUpdatePosition && ClientData->LastAckedMove.IsValid()
		&& ClientData->LastAckedMove->TimeStamp == MoveResponse.ClientAdjustment.TimeStamp)
	{
		SprintStamina = static_cast<const FRSCharacterMoveResponseDataContainer&>(MoveResponse).SprintStamina;
	}
}
