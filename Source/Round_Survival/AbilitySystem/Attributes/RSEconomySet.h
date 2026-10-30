// Round Survival

#pragma once

#include "AbilitySystem/Attributes/RSHealthSet.h"

#include "RSEconomySet.generated.h"

struct FGameplayEffectModCallbackData;

/**
 * Scrap, the match currency. Player only.
 *
 * Scrap is an attribute rather than a plain replicated integer so that every award and every
 * purchase is a gameplay effect. A phase-2 station that doubles income is then a single effect
 * modifying ScrapGainMultiplier, with no code aware that it exists.
 *
 * ScrapGain is a meta attribute: awards write to it, and PostGameplayEffectExecute applies the
 * multiplier and moves the result into Scrap. Purchases modify Scrap directly through an
 * ability cost, so that spending is never multiplied by an income buff.
 */
UCLASS()
class ROUND_SURVIVAL_API URSEconomySet : public URSAttributeSet
{
	GENERATED_BODY()

public:
	URSEconomySet();

	ATTRIBUTE_ACCESSORS(URSEconomySet, Scrap);
	ATTRIBUTE_ACCESSORS(URSEconomySet, ScrapGainMultiplier);
	ATTRIBUTE_ACCESSORS(URSEconomySet, ScrapGain);

	/**
	 * Fired when an executed effect changes Scrap on the server, and from replication on clients —
	 * not when the attribute is written any other way. The HUD listens to the ability system's own
	 * attribute delegate instead, which covers every route.
	 */
	mutable FRSAttributeEvent OnScrapChanged;

	/**
	 * Fired on the server when scrap is awarded, carrying the amount actually granted after
	 * the multiplier. The HUD's "+50" popup follows OnScrapChanged instead, so that spending shows
	 * as well.
	 */
	mutable FRSAttributeEvent OnScrapAwarded;

protected:
	UFUNCTION()
	void OnRep_Scrap(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ScrapGainMultiplier(const FGameplayAttributeData& OldValue);

	//~UAttributeSet interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	//~End of UAttributeSet interface

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	/** Replicated to every client: the HUD shows each player's balance, not only your own. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Economy", ReplicatedUsing = OnRep_Scrap, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Scrap;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Economy", ReplicatedUsing = OnRep_ScrapGainMultiplier, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ScrapGainMultiplier;

	/** Meta attribute. Awards write here; never replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Economy", meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData ScrapGain;

	/** Scrap before the currently executing effect, so the change can be reported. */
	float ScrapBeforeAttributeChange = 0.0f;
};
