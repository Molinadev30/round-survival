// Round Survival

#include "AbilitySystem/Attributes/RSCombatSet.h"

#include "Net/UnrealNetwork.h"

URSCombatSet::URSCombatSet()
	: OutgoingDamageMultiplier(1.0f)
	, IncomingDamageMultiplier(1.0f)
	, MoveSpeedMultiplier(1.0f)
{
}

void URSCombatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URSCombatSet, OutgoingDamageMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URSCombatSet, IncomingDamageMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URSCombatSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void URSCombatSet::OnRep_OutgoingDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSCombatSet, OutgoingDamageMultiplier, OldValue);
}

void URSCombatSet::OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSCombatSet, IncomingDamageMultiplier, OldValue);
}

void URSCombatSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URSCombatSet, MoveSpeedMultiplier, OldValue);

	OnMoveSpeedMultiplierChanged.Broadcast(nullptr, nullptr, nullptr,
		GetMoveSpeedMultiplier() - OldValue.GetCurrentValue(), OldValue.GetCurrentValue(), GetMoveSpeedMultiplier());
}

void URSCombatSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void URSCombatSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void URSCombatSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// Broadcast on the authority too. OnRep only runs on clients, and on a listen server the
	// host has to update its own movement speed like everyone else.
	if (Attribute == GetMoveSpeedMultiplierAttribute() && OldValue != NewValue)
	{
		OnMoveSpeedMultiplierChanged.Broadcast(nullptr, nullptr, nullptr, NewValue - OldValue, OldValue, NewValue);
	}
}

void URSCombatSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetOutgoingDamageMultiplierAttribute() || Attribute == GetIncomingDamageMultiplierAttribute())
	{
		// Zero is legitimate on both sides: a disarming debuff and an invulnerability window
		// are the same clamp. Negative would turn damage into healing.
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		// A zero multiplier would freeze the pawn, so a stacking slow that reaches zero is a
		// soft-lock. The floor is deliberate.
		NewValue = FMath::Max(NewValue, 0.01f);
	}
}
