// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility_Melee.h"

#include "RSGameplayAbility_ZombieMelee.generated.h"

/**
 * The enemy's swing at a player.
 *
 * Adds one thing to the melee it inherits, and it is the thing that makes a window dangerous: the
 * barricade this enemy came in through does not count as cover. Standing at a boarded window and
 * repairing it has to cost something, and without this it costs nothing — the melee's line of
 * sight trace runs on the visibility channel, the barricade frame blocks it, and every swing
 * through the boards would be quietly thrown away while the player rebuilt them at arm's length.
 *
 * The exception is this enemy's own window and nothing else, so no wall anywhere in the level
 * becomes swingable through. An enemy that has crossed and cleared its entry has no exception
 * left, which is correct: inside the room it is fighting in the open like everything else.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_ZombieMelee : public URSGameplayAbility_Melee
{
	GENERATED_BODY()

protected:
	//~URSGameplayAbility_Melee interface
	virtual void GetLineOfSightExceptions(TArray<AActor*>& OutActors) const override;
	virtual UAnimMontage* GetAttackMontage() const override;
	//~End of URSGameplayAbility_Melee interface

	/**
	 * The swing of an enemy that has lost a leg (`RS.State.Crawling`): from the floor, with its own
	 * `RS Melee Window`. Empty, a crawler swings with `AttackMontage`.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee")
	TObjectPtr<UAnimMontage> CrawlingAttackMontage;
};
