// Round Survival

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "AbilitySystem/Costs/RSAbilityCost.h"
#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "Core/RSPlayerController.h"

URSGameplayAbility::URSGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Instanced per actor rather than per execution: abilities here carry state across
	// activations (spread heat, hold progress) and re-instancing would throw it away.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// The ability object itself is not replicated; its effects, cues and target data are.
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;

	// Player abilities predict locally and are corrected by the server. Enemy abilities
	// override this to ServerOnly in their own constructors — nothing an enemy does is ever
	// predicted, because with 64 of them the prediction traffic is pure waste.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;
}

URSAbilitySystemComponent* URSGameplayAbility::GetRSAbilitySystemComponentFromActorInfo() const
{
	return CurrentActorInfo ? Cast<URSAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}

ARSPlayerController* URSGameplayAbility::GetRSPlayerControllerFromActorInfo() const
{
	return CurrentActorInfo ? Cast<ARSPlayerController>(CurrentActorInfo->PlayerController.Get()) : nullptr;
}

bool URSGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ActorInfo == nullptr || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool URSGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) || ActorInfo == nullptr)
	{
		return false;
	}

	// Every additional cost has to pass. Checking them all before charging any is what keeps a
	// two-cost ability from spending ammo and then failing on scrap.
	for (const TObjectPtr<URSAbilityCost>& Cost : AdditionalCosts)
	{
		if (Cost != nullptr && !Cost->CheckCost(this, Handle, ActorInfo, OptionalRelevantTags))
		{
			return false;
		}
	}

	return true;
}

void URSGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	check(ActorInfo);

	for (TObjectPtr<URSAbilityCost> Cost : AdditionalCosts)
	{
		if (Cost != nullptr && !Cost->ShouldOnlyApplyCostOnHit())
		{
			Cost->ApplyCost(this, Handle, ActorInfo, ActivationInfo);
		}
	}
}
