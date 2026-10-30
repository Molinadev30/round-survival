// Round Survival

#include "Enemies/StateTree/RSSTTask_PlayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Enemies/RSZombieController.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

namespace
{
	/**
	 * How soon after the last swing ended a new one counts as following straight on from it. Long
	 * enough for the state to go round through the approach and back, short enough that an enemy
	 * that stood about first starts from rest.
	 */
	constexpr double FollowOnWindow = 0.5;

	ARSZombieController* GetController(AActor* Actor)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		return Pawn ? Cast<ARSZombieController>(Pawn->GetController()) : nullptr;
	}

	/** The spec this task activated, or null once the ability has ended or been removed. */
	FGameplayAbilitySpec* FindRunningSpec(UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle)
	{
		return (ASC && Handle.IsValid()) ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	}
}

FRSSTTask_PlayAbility::FRSSTTask_PlayAbility()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FRSSTTask_PlayAbility::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ActivatedHandle = FGameplayAbilitySpecHandle();
	// Only from rest. A swing that follows straight on from the last one goes at once: a pause
	// there drops the body back to idle for a moment between two blows. Once they are swinging,
	// the montage's own rate variance keeps them out of step. Told apart by time rather than by
	// whether the last montage is still playing, because a montage that blends out through
	// inertialization has stopped by the time the next swing asks.
	const ARSZombieController* Controller = GetController(InstanceData.Actor);
	const UWorld* World = InstanceData.Actor ? InstanceData.Actor->GetWorld() : nullptr;
	const bool bFollowsOn = Controller && World && World->GetTimeSeconds() - Controller->LastPlayedAbilityEndTime < FollowOnWindow;

	InstanceData.RemainingDelay = (!bFollowsOn && InstanceData.MaxStartDelay > 0.0f) ? FMath::FRandRange(0.0f, InstanceData.MaxStartDelay) : 0.0f;

	// Waiting first: the tick activates once the wait runs out. A state with nothing to activate
	// still fails now rather than after the wait.
	if (InstanceData.RemainingDelay > 0.0f)
	{
		const bool bCanActivate = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor) && InstanceData.AbilityTag.IsValid();
		return bCanActivate ? EStateTreeRunStatus::Running : Activate(Context);
	}

	return Activate(Context);
}

EStateTreeRunStatus FRSSTTask_PlayAbility::Activate(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	if (!ASC || !InstanceData.AbilityTag.IsValid())
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("Play Ability has no ability system or no tag."));
		return EStateTreeRunStatus::Failed;
	}

	// Resolved to a concrete spec rather than fired with TryActivateAbilitiesByTag, because the
	// handle is what lets this task know later whether the ability is still running.
	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(InstanceData.AbilityTag), MatchingSpecs);

	for (const FGameplayAbilitySpec* Spec : MatchingSpecs)
	{
		if (Spec && ASC->TryActivateAbility(Spec->Handle))
		{
			InstanceData.ActivatedHandle = Spec->Handle;
			return EStateTreeRunStatus::Running;
		}
	}

	// Nothing granted matched, or everything that did refused: on cooldown, blocked by a tag, or
	// out of resources. A failure the tree can transition on, not an error.
	UE_VLOG(Context.GetOwner(), LogStateTree, Verbose, TEXT("Play Ability could not activate %s."), *InstanceData.AbilityTag.ToString());
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FRSSTTask_PlayAbility::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.RemainingDelay > 0.0f)
	{
		InstanceData.RemainingDelay -= DeltaTime;
		return InstanceData.RemainingDelay > 0.0f ? EStateTreeRunStatus::Running : Activate(Context);
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	const FGameplayAbilitySpec* Spec = FindRunningSpec(ASC, InstanceData.ActivatedHandle);

	// A spec that is gone was revoked with the pawn's kit; one that is no longer active has run its
	// course. Either way the attack is over and the state has done its job.
	return (Spec && Spec->IsActive()) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FRSSTTask_PlayAbility::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstanceData.Actor);
	if (FGameplayAbilitySpec* Spec = FindRunningSpec(ASC, InstanceData.ActivatedHandle))
	{
		if (Spec->IsActive())
		{
			// Left early — the target died, or something more urgent came up. The swing goes with it.
			ASC->CancelAbilityHandle(InstanceData.ActivatedHandle);
		}
	}

	// Only a swing that actually ran counts: a state left while still waiting to start one has
	// nothing for the next to follow on from.
	if (InstanceData.ActivatedHandle.IsValid())
	{
		ARSZombieController* Controller = GetController(InstanceData.Actor);
		const UWorld* World = InstanceData.Actor ? InstanceData.Actor->GetWorld() : nullptr;
		if (Controller && World)
		{
			Controller->LastPlayedAbilityEndTime = World->GetTimeSeconds();
		}
	}

	InstanceData.ActivatedHandle = FGameplayAbilitySpecHandle();
}
