// Round Survival

#include "Enemies/Abilities/RSGameplayAbility_ZombieReach.h"

#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "Interaction/RSBarricade.h"
#include "RSGameplayTags.h"

URSGameplayAbility_ZombieReach::URSGameplayAbility_ZombieReach(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Measured from the gap, not from the body: as far as the hand visibly gets past the window.
	// The swept sphere adds its own radius on top, so a player whose chest is up against the
	// boards is well inside it and one standing a pace back is not.
	Range = 40.0f;

	// The gaps are in the boards, a metre up. A crawler cannot get a hand to one; at a window it
	// tears at the boards from the floor instead.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Crawling);
}

bool URSGameplayAbility_ZombieReach::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// No slot, no gap. An enemy still queueing behind the window has nothing to reach through.
	FVector Gap;
	const ARSBarricade* Barricade = nullptr;
	return FindReachGap(Gap, Barricade);
}

bool URSGameplayAbility_ZombieReach::GetSweepFrame(FVector& OutOrigin, FVector& OutForward) const
{
	const ARSBarricade* Barricade = nullptr;
	if (!FindReachGap(OutOrigin, Barricade))
	{
		return false;
	}

	OutForward = Barricade->GetInwardDirection();
	return true;
}

bool URSGameplayAbility_ZombieReach::FindReachGap(FVector& OutGap, const ARSBarricade*& OutBarricade) const
{
	const ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(GetAvatarActorFromActorInfo());
	const ARSZombieController* Controller = Enemy ? Cast<ARSZombieController>(Enemy->GetController()) : nullptr;
	const ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	if (!Controller || !IsValid(Barricade) || !Barricade->GetReachPoint(Controller->BarricadeSlotIndex, OutGap))
	{
		return false;
	}

	OutBarricade = Barricade;
	return true;
}

UAnimMontage* URSGameplayAbility_ZombieReach::GetAttackMontage() const
{
	const ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(GetAvatarActorFromActorInfo());
	const ARSZombieController* Controller = Enemy ? Cast<ARSZombieController>(Enemy->GetController()) : nullptr;
	const ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	if (MirroredAttackMontage && Controller && IsValid(Barricade) && Barricade->IsReachMirrored(Controller->BarricadeSlotIndex))
	{
		return MirroredAttackMontage;
	}

	return Super::GetAttackMontage();
}
