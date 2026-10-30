// Round Survival

#include "AbilitySystem/Tasks/RSAbilityTask_Lunge.h"

#include "AbilitySystemComponent.h"
#include "Core/RSRootMotionSource_Lunge.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RSLogChannels.h"

URSAbilityTask_Lunge::URSAbilityTask_Lunge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSimulatedTask = false;
}

URSAbilityTask_Lunge* URSAbilityTask_Lunge::Lunge(UGameplayAbility* OwningAbility, AActor* Target, float Duration, float StopDistance,
	float MaxSpeed, float FinishMaxSpeed)
{
	URSAbilityTask_Lunge* Task = NewAbilityTask<URSAbilityTask_Lunge>(OwningAbility, TEXT("RSLunge"));
	Task->ForceName = TEXT("RSLunge");
	Task->Target = Target;
	Task->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
	Task->StopDistance = StopDistance;
	Task->MaxSpeed = MaxSpeed;
	Task->FinishVelocityMode = ERootMotionFinishVelocityMode::ClampVelocity;
	Task->FinishClampVelocity = FinishMaxSpeed;
	Task->SharedInitAndApply();
	return Task;
}

void URSAbilityTask_Lunge::SharedInitAndApply()
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	MovementComponent = ASC ? Cast<UCharacterMovementComponent>(ASC->AbilityActorInfo->MovementComponent.Get()) : nullptr;
	if (!MovementComponent.IsValid() || !Target.IsValid())
	{
		UE_LOG(LogRSAbility, Warning, TEXT("[%s] lunge with no movement component or no target; nothing moves."), *GetNameSafe(Ability));
		return;
	}

	StartTime = GetWorld()->GetTimeSeconds();
	EndTime = StartTime + Duration;

	TSharedPtr<FRSRootMotionSource_Lunge> Source = MakeShared<FRSRootMotionSource_Lunge>();
	Source->InstanceName = ForceName;
	Source->Priority = 900;
	Source->Duration = Duration;
	Source->StopDistance = StopDistance;
	Source->MaxSpeed = MaxSpeed;
	Source->TargetLocation = Target->GetActorLocation();
	Source->TargetVelocity = Target->GetVelocity();
	Source->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
	Source->FinishVelocityParams.Mode = FinishVelocityMode;
	Source->FinishVelocityParams.ClampVelocity = FinishClampVelocity;
	RootMotionSourceID = MovementComponent->ApplyRootMotionSource(Source);
}

bool URSAbilityTask_Lunge::UpdateTarget()
{
	const AActor* TargetActor = Target.Get();
	if (!TargetActor || !MovementComponent.IsValid())
	{
		return false;
	}

	const TSharedPtr<FRootMotionSource> Source = MovementComponent->GetRootMotionSourceByID(RootMotionSourceID);
	if (Source.IsValid() && Source->GetScriptStruct() == FRSRootMotionSource_Lunge::StaticStruct())
	{
		FRSRootMotionSource_Lunge* Lunge = static_cast<FRSRootMotionSource_Lunge*>(Source.Get());
		Lunge->TargetLocation = TargetActor->GetActorLocation();
		Lunge->TargetVelocity = TargetActor->GetVelocity();
	}
	return true;
}

void URSAbilityTask_Lunge::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);

	const AActor* Avatar = GetAvatarActor();
	const bool bTargetThere = UpdateTarget();
	const bool bArrived = bTargetThere && Avatar
		&& FVector::DistSquared2D(Avatar->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(StopDistance);

	if (!Avatar || !bTargetThere || bArrived || HasTimedOut())
	{
		bIsFinished = true;
		EndTask();
	}
}

void URSAbilityTask_Lunge::OnDestroy(bool bInOwnerFinished)
{
	if (MovementComponent.IsValid())
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy(bInOwnerFinished);
}
