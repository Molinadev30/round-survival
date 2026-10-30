// Round Survival

#include "Enemies/Abilities/RSGameplayAbility_FallToCrawl.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

URSGameplayAbility_FallToCrawl::URSGameplayAbility_FallToCrawl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// No prediction. Where root motion puts a falling body is the server's call.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// Started by the loss itself, not by the State Tree: the tree has no state for falling over.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = RSGameplayTags::State_Crawling;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(Trigger);

	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Zombie_AttackPlayer);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Zombie_AttackBarricade);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Zombie_ReachThrough);

	// Nothing new until the body is on the floor, a climb included: one started mid-fall would
	// warp a body that is still going down.
	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Zombie_AttackPlayer);
	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Zombie_AttackBarricade);
	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Zombie_ReachThrough);
	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Zombie_TraverseWindow);

	ActivationBlockedTags.AddTag(RSGameplayTags::State_Dead);
}

void URSGameplayAbility_FallToCrawl::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	// Mid-climb: the body falls once it is through. The blocks above already hold everything else
	// back while it waits.
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && ASC->HasMatchingGameplayTag(RSGameplayTags::State_TraversingWindow))
	{
		UAbilityTask_WaitGameplayTagRemoved* WaitTask = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(this,
			RSGameplayTags::State_TraversingWindow);
		WaitTask->Removed.AddDynamic(this, &ThisClass::HandleClimbEnded);
		WaitTask->ReadyForActivation();
		return;
	}

	Fall();
}

void URSGameplayAbility_FallToCrawl::Fall()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
			/*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	// Kept by the movement component from here on: it crouches on its next update, and again after
	// anything that stands it up for a while, like the flying a climb is done in.
	Character->Crouch();

	if (!FallMontage)
	{
		// Down without the drop. Still a crawler, which is what the loss is about.
		UE_LOG(LogRSAbility, Warning, TEXT("[%s] has no fall montage; [%s] drops to a crawl without one."),
			*GetName(), *Character->GetName());
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
			/*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this,
		NAME_None, FallMontage);

	// Blend out is on the floor: the crawl the graph is already in takes over from there.
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->ReadyForActivation();

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] lost a leg and is going down to crawl."), *Character->GetName());
}

void URSGameplayAbility_FallToCrawl::HandleClimbEnded()
{
	Fall();
}

void URSGameplayAbility_FallToCrawl::HandleMontageEnded()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}
