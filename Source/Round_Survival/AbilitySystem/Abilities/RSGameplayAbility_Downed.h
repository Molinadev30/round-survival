// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "Engine/TimerHandle.h"

#include "RSGameplayAbility_Downed.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayEffectSpec;

/**
 * What happens to a player whose health runs out: they go down rather than die.
 *
 * A passive, granted with the player's default set, active from spawn and server only, like the
 * health regeneration it sits next to. It watches the health set and, when health reaches zero,
 * applies DownedEffect. Everything being down means lives in that effect: the RS.State.Downed tag
 * it grants, the crawl (a modifier on MoveSpeedMultiplier) and no scrap for anything done on the
 * floor (ScrapGainMultiplier to zero). The tag does the rest wherever it is read — regeneration
 * stops, enemies and spawn placement stop counting the player, switching weapons and interacting
 * are refused.
 *
 * **The bleed-out is a timer here, not the effect's duration**, because it stops while a teammate
 * is reviving: a revive hold puts RS.State.BeingRevived on the player (ARSPlayerCharacter, as an
 * interactable), and this pauses the clock for as long as that tag is on. A gameplay effect's
 * duration cannot be paused. The effect is infinite, and whatever takes it off — a completed
 * revive, or the bleed-out itself — stops the clock with it. The timer only exists on the server, so
 * every change to it is published on the player state (FRSBleedOutClock), where every machine's HUD
 * reads it.
 *
 * **Bleeding out is death.** DeadEffect grants RS.State.Dead, and the game mode, which watches that
 * tag, takes the pawn away and makes the player a spectator until the round ends. Once the match is
 * lost nobody dies any more: the scene holds as it is until the host restarts.
 *
 * **Going down costs scrap, and leaving a teammate on the floor costs everyone.** Down, the player
 * loses DownedScrapLossFraction of their own scrap, and whoever revives them is paid exactly that
 * (ARSPlayerCharacter, on the revive). Bled out, every teammate still alive loses
 * BleedOutScrapLossFraction of theirs. Both go straight to Scrap through ScrapLossEffect, never
 * through the income multiplier, which only scales what is earned.
 *
 * **Second Wind gets a player back up alone, in a solo match only.** Whether they have it is read
 * before DownedEffect goes on, because the station is lost the moment the player goes down (its
 * effect is removed by the downed tag, like every station's): reading it after would always find it
 * gone. Read first, it is used up by the very down it undoes. The player then carries
 * RS.State.SelfReviving — on before the downed tag, so the game mode never sees an instant with
 * nobody able to keep the match going — and a SelfReviveSeconds timer runs instead of the bleed-out.
 * When it runs out the player is revived by the same path as a revive with nobody reviving
 * (IRSInteractable::Interact on the pawn). Same published clock, flagged, so the HUD only renames it.
 * With anyone else in the match the down is an ordinary one: in co-op Second Wind is the genre's
 * other half, reviving teammates in half the time (URSHandlingSet::ReviveSpeedMultiplier, on the
 * station's effect), and none of it is read here.
 *
 * A downed player keeps the weapon in their hands and can fire and reload it.
 *
 * Damage to a downed player is already discarded by the health set, which refuses damage at zero
 * health, so there is no second down while the first one lasts.
 *
 * Enemies are never granted it: they die at zero health, in the pawn (ARSZombieCharacter).
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_Downed : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Downed(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Ends a running bleed-out now, by the same path the clock running out takes. The debug hook. */
	void BleedOutNow();

protected:
	//~UGameplayAbility interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End of UGameplayAbility interface

	/** Infinite, and grants RS.State.Downed. Applied when health reaches zero, removed by a revive. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Downed")
	TSubclassOf<UGameplayEffect> DownedEffect;

	/** Seconds a downed player lasts with nobody reviving them. The clock stops while someone is. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Downed", meta = (ForceUnits = s, ClampMin = 1.0))
	float BleedOutSeconds = 30.0f;

	/** Seconds a player with Second Wind, alone in the match, stays down before getting back up. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Downed", meta = (ForceUnits = s, ClampMin = 0.1))
	float SelfReviveSeconds = 5.0f;

	/**
	 * Abilities stopped the moment the player goes down. Those the tag only refuses to start would
	 * otherwise finish what they were doing: a purchase held to the end would still go through.
	 *
	 * A weapon switch is deliberately not here. It finishes in under a second, and cancelled
	 * halfway it would leave the weapon in hand playing its holster with nothing drawn after it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Downed")
	FGameplayTagContainer CancelAbilitiesWhenDowned;

	/** Infinite, and grants RS.State.Dead. Applied when the bleed-out runs out; taken off by a respawn. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Downed")
	TSubclassOf<UGameplayEffect> DeadEffect;

	/** Instant, additive on Scrap, magnitude from RS.SetByCaller.Scrap: what every scrap penalty applies. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Downed")
	TSubclassOf<UGameplayEffect> ScrapLossEffect;

	/** Share of their own scrap a player loses going down, and what their reviver is paid. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Downed", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DownedScrapLossFraction = 0.05f;

	/** Share of their scrap every teammate still alive loses when this player bleeds out. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Downed", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float BleedOutScrapLossFraction = 0.10f;

private:
	void HandleOutOfHealth(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);

	/** Downed taken off, by whatever: the clock has nothing left to count. */
	void HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Someone started or stopped reviving: the clock stops while anyone is. */
	void HandleBeingRevivedTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** The clock ran out with nobody reviving: dead, and the team pays for it. */
	void HandleBledOut();

	/** Whether going down now gets this player back up alone: Second Wind, and nobody else in the match. */
	bool ShouldSelfRevive(const UAbilitySystemComponent& ASC) const;

	/** Second Wind's clock ran out: up, as a revive with nobody reviving. */
	void HandleSelfRevived();

	/**
	 * Copies whichever timer is running as it stands now — running, held or gone — onto the player
	 * state.
	 */
	void PublishBleedOutClock() const;

	/**
	 * Takes Fraction of one player's scrap, rounded down, through ScrapLossEffect with Reason on the
	 * spec for the HUD. Returns how much was taken.
	 */
	int32 TakeScrapFraction(UAbilitySystemComponent& ASC, float Fraction, const FGameplayTag& Reason) const;

	FActiveGameplayEffectHandle DownedEffectHandle;
	FTimerHandle BleedOutTimerHandle;
	FTimerHandle SelfReviveTimerHandle;

	/** This down put RS.State.SelfReviving on, and has to take it off when the down ends. */
	bool bSelfReviving = false;

	FDelegateHandle OutOfHealthHandle;
	FDelegateHandle DownedTagHandle;
	FDelegateHandle BeingRevivedTagHandle;
};
