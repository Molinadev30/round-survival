// Round Survival

#include "AbilitySystem/Attributes/RSHandlingSet.h"

#include "Net/UnrealNetwork.h"

URSHandlingSet::URSHandlingSet()
	: ReloadSpeedMultiplier(1.0f)
	, FireRateMultiplier(1.0f)
	, ReviveSpeedMultiplier(1.0f)
	, SprintDurationMultiplier(1.0f)
{
}

void URSHandlingSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Owner only: these scale first-person montages that no other connection plays, the shooter's
	// and the reviver's own clocks and the sprinter's own moves, which the server keeps its own copy of.
	DOREPLIFETIME_CONDITION_NOTIFY(URSHandlingSet, ReloadSpeedMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URSHandlingSet, FireRateMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URSHandlingSet, ReviveSpeedMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URSHandlingSet, SprintDurationMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
}

void URSHandlingSet::OnRep_ReloadSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSHandlingSet, ReloadSpeedMultiplier, OldValue);
}

void URSHandlingSet::OnRep_FireRateMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSHandlingSet, FireRateMultiplier, OldValue);
}

void URSHandlingSet::OnRep_ReviveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSHandlingSet, ReviveSpeedMultiplier, OldValue);
}

void URSHandlingSet::OnRep_SprintDurationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSHandlingSet, SprintDurationMultiplier, OldValue);
}

void URSHandlingSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void URSHandlingSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void URSHandlingSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetReloadSpeedMultiplierAttribute() || Attribute == GetFireRateMultiplierAttribute()
		|| Attribute == GetReviveSpeedMultiplierAttribute() || Attribute == GetSprintDurationMultiplierAttribute())
	{
		// A zero play rate is a reload that never finishes, a zero fire rate an infinite time between
		// shots and a zero revive speed a hold that never ends; all read as frozen with no way out. A
		// zero sprint is no sprint, which belongs to no buff. The floor is deliberate.
		NewValue = FMath::Max(NewValue, 0.01f);
	}
}
