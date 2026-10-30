// Round Survival

#pragma once

#include "Abilities/GameplayAbility.h"

#include "RSGameplayAbility.generated.h"

class URSAbilityCost;
class URSAbilitySystemComponent;
class ARSPlayerController;

/**
 * How an ability reacts to the input tag bound to it.
 */
UENUM(BlueprintType)
enum class ERSAbilityActivationPolicy : uint8
{
	/** Activate once when the input is pressed. Firing a semi-automatic shot, reloading. */
	OnInputTriggered,

	/** Stay active while the input is held and end when it is released. Automatic fire, aiming. */
	WhileInputActive,

	/** Activate as soon as the ability is granted and an avatar exists. Passives. */
	OnSpawn,
};

/**
 * Base gameplay ability for the project.
 *
 * Adds two things to the engine class: an activation policy, so input binding is data rather
 * than a switch statement somewhere in the character, and a list of instanced costs, so an
 * ability can charge ammo or scrap without those living on an attribute set.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	ERSAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	/**
	 * Whether a held input should activate this ability again whenever it is not already running.
	 *
	 * The activation policy answers it by default. An ability whose answer depends on what granted
	 * it overrides this — the fire ability asks its weapon, so automatic fire is a weapon stat and
	 * one ability serves semi-automatic and automatic weapons alike. Called on the class default
	 * object, so the spec is how an override finds its source.
	 */
	virtual bool ShouldActivateWhileInputHeld(const FGameplayAbilitySpec& Spec, const FGameplayAbilityActorInfo* ActorInfo) const
	{
		return ActivationPolicy == ERSAbilityActivationPolicy::WhileInputActive;
	}

	UFUNCTION(BlueprintCallable, Category = "RS|Ability")
	URSAbilitySystemComponent* GetRSAbilitySystemComponentFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "RS|Ability")
	ARSPlayerController* GetRSPlayerControllerFromActorInfo() const;

protected:
	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	//~End of UGameplayAbility interface

	/** Governs how the bound input tag activates this ability. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Ability Activation")
	ERSAbilityActivationPolicy ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;

	/**
	 * Costs checked before activation and charged on activation.
	 *
	 * Instanced, so each ability owns its own configured copies.
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "RS|Costs")
	TArray<TObjectPtr<URSAbilityCost>> AdditionalCosts;
};
