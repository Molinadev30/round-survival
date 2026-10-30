// Round Survival

#include "AbilitySystem/Attributes/RSEconomySet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

URSEconomySet::URSEconomySet()
	: Scrap(0.0f)
	, ScrapGainMultiplier(1.0f)
{
}

void URSEconomySet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Scrap goes to everyone: the HUD lists every player's balance, as the genre does. The
	// multiplier stays with its owner — nobody else renders it.
	DOREPLIFETIME_CONDITION_NOTIFY(URSEconomySet, Scrap, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URSEconomySet, ScrapGainMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
}

void URSEconomySet::OnRep_Scrap(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSEconomySet, Scrap, OldValue);

	OnScrapChanged.Broadcast(nullptr, nullptr, nullptr,
		GetScrap() - OldValue.GetCurrentValue(), OldValue.GetCurrentValue(), GetScrap());
}

void URSEconomySet::OnRep_ScrapGainMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSEconomySet, ScrapGainMultiplier, OldValue);
}

bool URSEconomySet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	// Taken here, before the modifier lands. A purchase modifies Scrap itself, so by
	// PostGameplayEffectExecute the balance has already moved and "before" would equal "after".
	ScrapBeforeAttributeChange = GetScrap();
	return true;
}

void URSEconomySet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetEffectContext();
	AActor* Instigator = EffectContext.GetOriginalInstigator();
	AActor* Causer = EffectContext.GetEffectCauser();

	if (Data.EvaluatedData.Attribute == GetScrapGainAttribute())
	{
		// Awards go through the multiplier. Spending does not: an income buff must never make
		// a wall locker cheaper.
		const float Granted = FMath::RoundToFloat(GetScrapGain() * GetScrapGainMultiplier());

		SetScrap(FMath::Max(GetScrap() + Granted, 0.0f));
		SetScrapGain(0.0f);

		OnScrapAwarded.Broadcast(Instigator, Causer, &Data.EffectSpec, Granted,
			ScrapBeforeAttributeChange, GetScrap());
	}
	else if (Data.EvaluatedData.Attribute == GetScrapAttribute())
	{
		SetScrap(FMath::Max(GetScrap(), 0.0f));
	}

	if (GetScrap() != ScrapBeforeAttributeChange)
	{
		OnScrapChanged.Broadcast(Instigator, Causer, &Data.EffectSpec, GetScrap() - ScrapBeforeAttributeChange,
			ScrapBeforeAttributeChange, GetScrap());
	}
}

void URSEconomySet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void URSEconomySet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void URSEconomySet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetScrapAttribute())
	{
		// Scrap never goes negative. A purchase that cannot be afforded is rejected by the
		// ability cost before it reaches the attribute, so a clamp here means a bug upstream.
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetScrapGainMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}
