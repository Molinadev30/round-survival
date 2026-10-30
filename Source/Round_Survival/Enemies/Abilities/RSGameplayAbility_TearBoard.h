// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_TearBoard.generated.h"

class ARSBarricade;
class UAnimMontage;
struct FGameplayEventData;

/**
 * One swing at a window, which takes a board off it.
 *
 * Deliberately **not** a `URSGameplayAbility_Melee` subclass, despite looking like one from the
 * outside. A melee swing has to find out what it hit: it sweeps a volume, filters by team, traces
 * line of sight and applies a damage effect to whatever survived all three. None of that applies
 * here. The attacker is standing in a slot on a barricade it was handed at birth, so the target is
 * known before the animation starts and there is nothing to search for — and the boards carry no
 * collision anyway, so a sweep would find the frame or nothing at all.
 *
 * What the two do share is the cadence, and they share it by construction rather than by
 * inheritance: the montage is the attack, the `RS Melee Window` notify state on it says when the
 * blow lands, and the ability ends when the animation does. One montage retimed changes how fast
 * a window comes down, with no cooldown effect to keep in step with it.
 *
 * The barricade is not damaged, it loses a board. `ARSBarricade` has no health and no ability
 * system on purpose — a board is a count, its visuals are driven off that count, and the repair
 * that puts it back is the same integer going the other way.
 *
 * Server only and unpredicted, like everything else an enemy does.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_TearBoard : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_TearBoard(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End of UGameplayAbility interface

	/** The window this swing is aimed at: the one the attacker came in through. */
	ARSBarricade* GetTargetBarricade() const;

	/** `CrawlingAttackMontage` for a crawler, `AttackMontage` otherwise. */
	UAnimMontage* GetAttackMontage() const;

	/** The animation says the blow has landed. Takes the board. */
	UFUNCTION()
	void HandleWindowOpened(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageFinished();

	/** The swing. Its `RS Melee Window` notify state is what decides when the board comes off. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Barricade Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * The swing of an enemy that has lost a leg (`RS.State.Crawling`), clawing at the window from
	 * the floor. A crawler still takes boards: one that could not would stand in its slot for good
	 * and jam the window for everyone queueing behind it. Empty, it swings with `AttackMontage`.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Barricade Attack")
	TObjectPtr<UAnimMontage> CrawlingAttackMontage;

	/**
	 * Boards taken per swing.
	 *
	 * Data rather than a constant because this is the dial that sets how long a repaired window
	 * buys the player, and it is the obvious thing for a heavier enemy to raise later.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Barricade Attack", meta = (ClampMin = 1))
	int32 BoardsPerSwing = 1;

	/** Playback rate of the montage, which is also how fast the attacker can swing again. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Barricade Attack", meta = (ClampMin = 0.01))
	float MontageRate = 1.0f;

	/**
	 * How much each swing's playback rate strays from `MontageRate`, either way: 0.1 plays each one
	 * somewhere between 0.9 and 1.1 times as fast. Attackers standing side by side drift out of
	 * step instead of swinging in unison, and the average rhythm stays where `MontageRate` puts it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Barricade Attack", meta = (ClampMin = 0.0, ClampMax = 0.5))
	float MontageRateVariance = 0.0f;
};
