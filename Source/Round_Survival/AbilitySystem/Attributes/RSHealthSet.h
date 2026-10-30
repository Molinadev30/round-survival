// Round Survival

#pragma once

#include "AbilitySystem/Attributes/RSAttributeSet.h"

#include "RSHealthSet.generated.h"

struct FGameplayEffectModCallbackData;

/**
 * Broadcast when health changes or runs out.
 *
 * @param EffectInstigator  Actor that caused the change. Null on clients for replicated changes.
 * @param EffectCauser      Physical actor responsible, e.g. the weapon.
 * @param EffectSpec        The full spec. Null when the change did not come from an effect.
 * @param Magnitude         Raw magnitude before clamping.
 * @param OldValue          Value before the change.
 * @param NewValue          Value after the change.
 */
DECLARE_MULTICAST_DELEGATE_SixParams(FRSAttributeEvent, AActor* /*EffectInstigator*/, AActor* /*EffectCauser*/,
	const struct FGameplayEffectSpec* /*EffectSpec*/, float /*Magnitude*/, float /*OldValue*/, float /*NewValue*/);

/**
 * Health, and the two meta attributes that write to it.
 *
 * Damage and Healing exist so that no system outside this set ever touches Health directly:
 * an execution writes a magnitude into the meta attribute, PostGameplayEffectExecute converts
 * it into a clamped Health change and resets it to zero. That keeps clamping, death detection
 * and the "who killed me" bookkeeping in exactly one place.
 */
UCLASS()
class ROUND_SURVIVAL_API URSHealthSet : public URSAttributeSet
{
	GENERATED_BODY()

public:
	URSHealthSet();

	ATTRIBUTE_ACCESSORS(URSHealthSet, Health);
	ATTRIBUTE_ACCESSORS(URSHealthSet, MaxHealth);
	ATTRIBUTE_ACCESSORS(URSHealthSet, Healing);
	ATTRIBUTE_ACCESSORS(URSHealthSet, Damage);

	/** Fired on the server and on clients whenever Health changes. */
	mutable FRSAttributeEvent OnHealthChanged;

	/** Fired on the server and on clients whenever MaxHealth changes. */
	mutable FRSAttributeEvent OnMaxHealthChanged;

	/** Fired when Health reaches zero. The death ability listens for this. */
	mutable FRSAttributeEvent OnOutOfHealth;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	//~UAttributeSet interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	//~End of UAttributeSet interface

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	UPROPERTY(BlueprintReadOnly, Category = "RS|Health", ReplicatedUsing = OnRep_Health, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Health", ReplicatedUsing = OnRep_MaxHealth, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxHealth;

	/** Meta attribute. Written by healing executions, never replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Health", meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData Healing;

	/** Meta attribute. Written by the damage execution, never replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Health", meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData Damage;

	/** Guards against OnOutOfHealth firing more than once per death. */
	bool bOutOfHealth = false;

	/** Health before the currently executing effect, used to report the change accurately. */
	float HealthBeforeAttributeChange = 0.0f;
	float MaxHealthBeforeAttributeChange = 0.0f;
};
