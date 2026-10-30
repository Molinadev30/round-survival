// Round Survival

#include "AbilitySystem/Abilities/RSGameplayAbility_Melee.h"

#include "AbilitySystem/Tasks/RSAbilityTask_MeleeSweep.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

URSGameplayAbility_Melee::URSGameplayAbility_Melee(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// One instance per actor: the swing keeps state between its montage and its damage window, and
	// an enemy only ever has one swing in the air at a time.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// No prediction. The enemy's melee is decided by the server and the clients are told about it
	// through the montage and the health change, which they would have received anyway.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
}

void URSGameplayAbility_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	UAnimMontage* Montage = GetAttackMontage();
	if (!Montage)
	{
		UE_LOG(LogRSAbility, Error, TEXT("[%s] has no attack montage; a melee ability with no swing can never land."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	// Listen before playing: a window notify sitting on the very first frame of the montage would
	// otherwise fire before there was anybody to hear it.
	UAbilityTask_WaitGameplayEvent* WindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,
		RSGameplayTags::Event_Melee_Window, /*OptionalExternalTarget=*/ nullptr, /*OnlyTriggerOnce=*/ true);
	WindowTask->EventReceived.AddDynamic(this, &ThisClass::HandleWindowOpened);
	WindowTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this,
		NAME_None, Montage, MontageRate * FMath::FRandRange(1.0f - MontageRateVariance, 1.0f + MontageRateVariance));

	// The ability ends when the animation does, whichever way it ends. That is the whole cadence:
	// a missed swing still costs the attacker the rest of its montage.
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

void URSGameplayAbility_Melee::GetLineOfSightExceptions(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
}

bool URSGameplayAbility_Melee::GetSweepFrame(FVector& OutOrigin, FVector& OutForward) const
{
	return false;
}

void URSGameplayAbility_Melee::HandleWindowOpened(FGameplayEventData Payload)
{
	TArray<AActor*> Exceptions;
	GetLineOfSightExceptions(Exceptions);

	// The notify sends the window's length as the magnitude, so the sweep runs for exactly the
	// frames the animation says are dangerous and not one more.
	URSAbilityTask_MeleeSweep* SweepTask = URSAbilityTask_MeleeSweep::MeleeSweep(this, Payload.EventMagnitude,
		Range, SweepRadius, ConeHalfAngle, Exceptions, bDrawDebug);

	FVector SweepOrigin;
	FVector SweepForward;
	if (GetSweepFrame(SweepOrigin, SweepForward))
	{
		SweepTask->SetFixedFrame(SweepOrigin, SweepForward);
	}

	SweepTask->OnHit.AddDynamic(this, &ThisClass::HandleSweepHit);
	SweepTask->OnMissed.AddDynamic(this, &ThisClass::HandleSweepMissed);
	SweepTask->ReadyForActivation();
}

void URSGameplayAbility_Melee::HandleSweepHit(const FHitResult& Hit)
{
	AActor* Target = Hit.GetActor();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC || !DamageEffect)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
		GetCurrentActivationInfo(), DamageEffect, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// The impact goes on the context the spec already built. The damage execution reads it for the
	// hit location; melee has no ability source, so distance and hit zone both stay at one.
	SpecHandle.Data->GetContext().AddHitResult(Hit);

	// The amount rides on the spec rather than on the effect asset, so round scaling can raise it
	// without a second effect, exactly as the ranged damage already does.
	SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Damage, Damage);

	// The damage execution refuses player-on-player damage on its own; nothing here has to know
	// who is swinging at whom.
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] melee connected on [%s] for %.0f"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(Target), Damage);
}

void URSGameplayAbility_Melee::HandleSweepMissed()
{
	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] melee window closed empty."), *GetNameSafe(GetAvatarActorFromActorInfo()));
}

void URSGameplayAbility_Melee::HandleMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}
