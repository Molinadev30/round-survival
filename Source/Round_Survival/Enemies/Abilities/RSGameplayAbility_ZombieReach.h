// Round Survival

#pragma once

#include "Enemies/Abilities/RSGameplayAbility_ZombieMelee.h"

#include "RSGameplayAbility_ZombieReach.generated.h"

class ARSBarricade;

/**
 * An enemy at a barricade slot reaching through its gap at a player on the other side.
 *
 * The same swing as the melee it inherits — montage, damage window, one victim — thrown from a
 * different place. Each of a window's three slots owns one gap in the boards (see the layout's
 * `ReachPoints`), and the blow starts there and travels inward, rather than from the enemy's body
 * standing 80 cm out. That is what makes it readable: a player at the window is in danger in front
 * of the three holes and nowhere else, and backing off from the glass is what gets them clear.
 *
 * The enemy keeps its place at the window for the whole reach; it is a state of the barricade
 * branch, not the open-field attack, so it goes back to tearing the moment the player steps away.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_ZombieReach : public URSGameplayAbility_ZombieMelee
{
	GENERATED_BODY()

public:
	URSGameplayAbility_ZombieReach(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	//~End of UGameplayAbility interface

	//~URSGameplayAbility_Melee interface
	virtual bool GetSweepFrame(FVector& OutOrigin, FVector& OutForward) const override;
	virtual UAnimMontage* GetAttackMontage() const override;
	//~End of URSGameplayAbility_Melee interface

	/**
	 * The reach with the other arm, for slots the layout marks `bMirrored`. The same clip, in a
	 * montage that plays on the graph's mirrored slot, so which arm comes out is decided here, on
	 * the server, and reaches clients through ordinary montage replication.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Melee")
	TObjectPtr<UAnimMontage> MirroredAttackMontage;

	/** The gap this enemy's slot owns, and the window it belongs to. False when it holds no slot. */
	bool FindReachGap(FVector& OutGap, const ARSBarricade*& OutBarricade) const;
};
