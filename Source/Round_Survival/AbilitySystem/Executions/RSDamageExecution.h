// Round Survival

#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "RSDamageExecution.generated.h"

/**
 * Turns a damage effect into a number and writes it to the target's Damage meta attribute.
 *
 *     base damage          SetByCaller RS.SetByCaller.Damage, from the weapon
 *   × distance falloff     the ability source, from the context origin to the impact
 *   × hit-zone multiplier  the ability source, from the physical material that was hit
 *   × OutgoingDamageMultiplier on the instigator, snapshotted when the spec is made
 *   × IncomingDamageMultiplier on the target, read when the effect lands
 *
 * This is the only place those factors meet. The attribute set then turns Damage into a Health
 * change, so neither half knows how the other works: a new weapon type is a new ability source,
 * and armour or round scaling is an effect on IncomingDamageMultiplier.
 *
 * It is also where damage is refused. Player-on-player damage is dropped here rather than in the
 * fire ability, so every future source of damage — explosives, traps — inherits the same rule
 * without having to remember it.
 */
UCLASS()
class ROUND_SURVIVAL_API URSDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	URSDamageExecution();

protected:
	//~UGameplayEffectExecutionCalculation interface
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
	//~End of UGameplayEffectExecutionCalculation interface
};
