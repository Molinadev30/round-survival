// Round Survival

#include "Enemies/Abilities/RSGameplayAbility_TearBoard.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Enemies/RSZombieCharacter.h"
#include "Interaction/RSBarricade.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

URSGameplayAbility_TearBoard::URSGameplayAbility_TearBoard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// One instance per actor: the swing keeps state between its montage and its damage window, and
	// an enemy only ever has one swing in the air at a time.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// No prediction. A board coming off is the server's call, and the clients hear about it through
	// the montage and the replicated board count they would have received anyway.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;
}

bool URSGameplayAbility_TearBoard::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// A window with nothing left on it is not something to swing at. The State Tree tests the same
	// thing before entering the state, but the ability answering for itself is what keeps the tree
	// from being the only thing standing between an enemy and an empty animation.
	const ARSBarricade* Barricade = GetTargetBarricade();

	return Barricade && !Barricade->IsBreached();
}

void URSGameplayAbility_TearBoard::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
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
		UE_LOG(LogRSAbility, Error, TEXT("[%s] has no attack montage; a swing with no animation has no window to land in."), *GetName());
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

	// The ability ends when the animation does, whichever way it ends — which is what makes the
	// montage the cadence of the whole attack.
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

ARSBarricade* URSGameplayAbility_TearBoard::GetTargetBarricade() const
{
	const ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(GetAvatarActorFromActorInfo());
	ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	// IsValid rather than a null check: the window can be torn out of the level while an enemy is
	// mid-swing at it, and the pointer it was handed at birth would outlive it by a frame.
	return IsValid(Barricade) ? Barricade : nullptr;
}

UAnimMontage* URSGameplayAbility_TearBoard::GetAttackMontage() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (CrawlingAttackMontage && ASC && ASC->HasMatchingGameplayTag(RSGameplayTags::State_Crawling))
	{
		return CrawlingAttackMontage;
	}

	return AttackMontage;
}

void URSGameplayAbility_TearBoard::HandleWindowOpened(FGameplayEventData Payload)
{
	ARSBarricade* Barricade = GetTargetBarricade();
	AActor* Attacker = GetAvatarActorFromActorInfo();

	if (!Barricade)
	{
		return;
	}

	// Asked for rather than assumed: another enemy at the same window can take the last board
	// while this swing is in the air, and `RemoveBoard` saying no is the answer to that.
	int32 Taken = 0;
	for (int32 Board = 0; Board < BoardsPerSwing; ++Board)
	{
		if (!Barricade->RemoveBoard(Attacker))
		{
			break;
		}

		++Taken;
	}

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] tore %d board(s) off [%s], %d left."),
		*GetNameSafe(Attacker), Taken, *GetNameSafe(Barricade), Barricade->GetBoardCount());
}

void URSGameplayAbility_TearBoard::HandleMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}
