// Round Survival

#include "Animation/RSAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/RSCharacterMovementComponent.h"
#include "Core/RSPlayerCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "RSGameplayTags.h"

URSAnimInstance::URSAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URSAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// The viewmodel is a component of the character, so both meshes resolve to the same pawn.
	OwningCharacter = Cast<ACharacter>(TryGetPawnOwner());

	if (OwningCharacter.IsValid())
	{
		OwningMovement = OwningCharacter->GetCharacterMovement();
	}
}

void URSAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Game thread only: reading gameplay tags touches the ability system's containers, which the
	// worker thread must not do while gameplay code may be writing to them.
	if (const AActor* OwnerActor = OwningCharacter.Get())
	{
		if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor))
		{
			bIsAiming = ASC->HasMatchingGameplayTag(RSGameplayTags::State_ADS);
			bIsReloading = ASC->HasMatchingGameplayTag(RSGameplayTags::State_Reloading);
			bIsSwitchingWeapon = ASC->HasMatchingGameplayTag(RSGameplayTags::State_SwitchingWeapon);
		}
	}

	if (const ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(OwningCharacter.Get()))
	{
		AimDownSightsAlpha = OwningPlayer->GetAimDownSightsAlpha();

		// At a constant rate, as the aim is, so going in and coming out take the same time. Game
		// thread: the replicated sprint state and the crouch flag are written there.
		const auto Ease = [DeltaSeconds](float Current, bool bTarget, float Seconds)
		{
			const float Target = bTarget ? 1.0f : 0.0f;
			return Seconds > KINDA_SMALL_NUMBER ? FMath::FInterpConstantTo(Current, Target, DeltaSeconds, 1.0f / Seconds) : Target;
		};
		SprintAlpha = Ease(SprintAlpha, OwningPlayer->IsSprinting(), SprintBlendSeconds);
		CrouchAlpha = Ease(CrouchAlpha, OwningPlayer->bIsCrouched, CrouchBlendSeconds);

		// A copy with no controller reads the replicated RemoteViewPitch, which arrives in steps of
		// 1.4 degrees at the net rate: it is eased so the body does not tick from one to the next.
		const float ViewPitch = FRotator::NormalizeAxis(OwningPlayer->GetBaseAimRotation().Pitch);
		const bool bExactPitch = OwningPlayer->GetController() != nullptr || AimPitchInterpSpeed <= 0.0f;
		AimPitch = bExactPitch ? ViewPitch : FMath::FInterpTo(AimPitch, ViewPitch, DeltaSeconds, AimPitchInterpSpeed);

		// A sprinting player carries the weapon rather than aiming it.
		BodyAimPitch = FMath::Lerp(AimPitch, SprintCarryPitch, SprintAlpha);
	}

	UpdateLeftHandGrip(DeltaSeconds);
}

void URSAnimInstance::SetLeftHandGripPoses(UAnimSequenceBase* IdlePose, UAnimSequenceBase* AimPose)
{
	bHasLeftHandGripPose = (IdlePose != nullptr);

	// Drawing a weapon without a foregrip keeps the old pose playing: its alpha is what fades it
	// out, and swapping it for nothing first would snap the hand instead.
	if (IdlePose)
	{
		LeftHandGripIdlePose = IdlePose;
		LeftHandGripAimPose = AimPose ? AimPose : IdlePose;
	}
}

void URSAnimInstance::ReleaseLeftHandGrip(float BlendOutSpeed)
{
	++LeftHandGripReleaseCount;
	LeftHandGripWindowBlendSpeed = BlendOutSpeed;
}

void URSAnimInstance::ReturnLeftHandGrip(float BlendReturnSpeed)
{
	LeftHandGripReleaseCount = FMath::Max(LeftHandGripReleaseCount - 1, 0);
	LeftHandGripWindowBlendSpeed = BlendReturnSpeed;
}

void URSAnimInstance::UpdateLeftHandGrip(float DeltaSeconds)
{
	// Not the reload or switch state tags: those last until the ability ends, by which time the
	// montage has blended out to the handguard hold, and the grip fading in only then shows that
	// hold first. The montage's own windows end while it is still moving the arm.
	const bool bHandFree = (LeftHandGripReleaseCount == 0);
	const float TargetAlpha = (bHasLeftHandGripPose && bHandFree) ? 1.0f : 0.0f;
	const float BlendSpeed = LeftHandGripWindowBlendSpeed.Get(LeftHandGripBlendSpeed);

	LeftHandGripAlpha = FMath::FInterpTo(LeftHandGripAlpha, TargetAlpha, DeltaSeconds, BlendSpeed);
}

void URSAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	const ACharacter* Character = OwningCharacter.Get();
	if (!Character)
	{
		return;
	}

	const FVector Velocity = Character->GetVelocity();

	// Horizontal only: falling should not read as running.
	Speed = Velocity.Size2D();
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, Character->GetActorRotation());

	VerticalSpeed = Velocity.Z;

	if (const UCharacterMovementComponent* Movement = OwningMovement.Get())
	{
		bIsInAir = Movement->IsFalling();
		bIsCrouched = Movement->IsCrouching();

		const URSCharacterMovementComponent* RSMovement = Cast<URSCharacterMovementComponent>(Movement);
		bIsSliding = RSMovement && RSMovement->IsSliding();

		// Normalised against max walk speed so the blend space reads the same whether the pawn is
		// slowed by a status effect or sped up by one.
		const float MaxSpeed = FMath::Max(Movement->GetMaxSpeed(), 1.0f);
		const FVector LocalVelocity = Character->GetActorRotation().UnrotateVector(Velocity);

		MoveForwardPercent = FMath::Clamp(LocalVelocity.X / MaxSpeed * 100.0f, -100.0f, 100.0f);
		MoveRightPercent = FMath::Clamp(LocalVelocity.Y / MaxSpeed * 100.0f, -100.0f, 100.0f);
	}
}
