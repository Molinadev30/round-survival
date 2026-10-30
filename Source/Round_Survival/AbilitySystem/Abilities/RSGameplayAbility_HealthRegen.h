// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"

#include "RSGameplayAbility_HealthRegen.generated.h"

class UGameplayEffect;
struct FGameplayEffectSpec;

/**
 * Health coming back on its own a short while after the last hit: the genre's only healing.
 *
 * A passive, granted with the player's default set, active from spawn and server only. The healing
 * itself is RegenEffect, an infinite periodic effect writing Healing that stops ticking while its
 * owner carries RS.State.RegenBlocked. This ability's one job is to put that tag back on for a
 * while after every drop in health, by applying RegenDelayEffect. The rate, the wait and the rule
 * that the tag holds regeneration off all live in those two effects, and nothing here ticks.
 *
 * Enemies are never granted it. Sixty-four periodic effects for a mechanic they do not have would
 * be pure cost.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_HealthRegen : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_HealthRegen(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UGameplayAbility interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End of UGameplayAbility interface

	/** Infinite and periodic, writing Healing, inhibited by RS.State.RegenBlocked. Applied for as long as this ability is active. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Regen")
	TSubclassOf<UGameplayEffect> RegenEffect;

	/** Has a duration and grants RS.State.RegenBlocked. Reapplied on every drop in health, so each hit restarts the wait. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Regen")
	TSubclassOf<UGameplayEffect> RegenDelayEffect;

private:
	void HandleHealthChanged(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);

	FActiveGameplayEffectHandle RegenEffectHandle;
	FDelegateHandle HealthChangedHandle;
};
