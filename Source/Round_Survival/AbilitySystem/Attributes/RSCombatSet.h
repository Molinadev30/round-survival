// Round Survival

#pragma once

#include "AbilitySystem/Attributes/RSHealthSet.h"

#include "RSCombatSet.generated.h"

/**
 * Combat modifiers shared by players and enemies.
 *
 * Everything here is persistent pawn state that gameplay effects stack: a buff station, a round
 * scaling effect or an enemy movement tier is a data change rather than a code change. Per-shot
 * numbers do not live here — the weapon's base damage travels on the effect spec as a SetByCaller,
 * because it belongs to the weapon and its Forge upgrade level, not to whoever is holding it.
 *
 * Damage is multiplied twice, once on each side of the exchange:
 *   - OutgoingDamageMultiplier on the instigator (Overcharge, a temporary damage buff). Not the
 *     Weapon Forge: its upgrade belongs to one weapon, and this would raise every weapon held
 *   - IncomingDamageMultiplier on the target (armour, round difficulty scaling)
 *
 * Nothing reads these as raw numbers outside the damage execution, except MoveSpeedMultiplier,
 * which the character pushes into movement through a listener.
 */
UCLASS()
class ROUND_SURVIVAL_API URSCombatSet : public URSAttributeSet
{
	GENERATED_BODY()

public:
	URSCombatSet();

	ATTRIBUTE_ACCESSORS(URSCombatSet, OutgoingDamageMultiplier);
	ATTRIBUTE_ACCESSORS(URSCombatSet, IncomingDamageMultiplier);
	ATTRIBUTE_ACCESSORS(URSCombatSet, MoveSpeedMultiplier);

	/** Fired when the movement multiplier changes so the character can push it into movement. */
	mutable FRSAttributeEvent OnMoveSpeedMultiplierChanged;

protected:
	UFUNCTION()
	void OnRep_OutgoingDamageMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);

	//~UAttributeSet interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	//~End of UAttributeSet interface

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	/** Applied to outgoing damage on the instigator, whatever it holds. Damage buffs live here; the Weapon Forge's upgrade is on the weapon. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Combat", ReplicatedUsing = OnRep_OutgoingDamageMultiplier, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData OutgoingDamageMultiplier;

	/** Applied to incoming damage on the target. Armour and difficulty scaling live here. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Combat", ReplicatedUsing = OnRep_IncomingDamageMultiplier, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData IncomingDamageMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Combat", ReplicatedUsing = OnRep_MoveSpeedMultiplier, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MoveSpeedMultiplier;
};
