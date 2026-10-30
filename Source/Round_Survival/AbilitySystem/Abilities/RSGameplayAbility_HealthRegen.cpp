// Round Survival

#include "AbilitySystem/Abilities/RSGameplayAbility_HealthRegen.h"

#include "AbilitySystem/Attributes/RSHealthSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "RSLogChannels.h"

URSGameplayAbility_HealthRegen::URSGameplayAbility_HealthRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// One instance for the whole life of the ability system: it holds the regen effect's handle
	// and a binding to the health set.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Health only changes on the server, and the effects it applies replicate on their own.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	ActivationPolicy = ERSAbilityActivationPolicy::OnSpawn;
}

void URSGameplayAbility_HealthRegen::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const URSHealthSet* HealthSet = ASC ? ASC->GetSet<URSHealthSet>() : nullptr;

	if (!HealthSet || !RegenEffect || !RegenDelayEffect)
	{
		UE_LOG(LogRSAbility, Error, TEXT("[%s] needs a health set on its owner and both regen effects set."), *GetNameSafe(this));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	RegenEffectHandle = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
		RegenEffect->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel(Handle, ActorInfo));

	HealthChangedHandle = HealthSet->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
}

void URSGameplayAbility_HealthRegen::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (const URSHealthSet* HealthSet = ASC->GetSet<URSHealthSet>())
		{
			HealthSet->OnHealthChanged.Remove(HealthChangedHandle);
		}

		ASC->RemoveActiveGameplayEffect(RegenEffectHandle);
	}

	HealthChangedHandle.Reset();
	RegenEffectHandle.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_HealthRegen::HandleHealthChanged(AActor* EffectInstigator, AActor* EffectCauser,
	const FGameplayEffectSpec* EffectSpec, float Magnitude, float OldValue, float NewValue)
{
	if (NewValue >= OldValue)
	{
		return;
	}

	// The delay stacks to one and refreshes, so while one is running a new hit only restarts it,
	// and the tag never drops in between.
	ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		RegenDelayEffect->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel());
}
