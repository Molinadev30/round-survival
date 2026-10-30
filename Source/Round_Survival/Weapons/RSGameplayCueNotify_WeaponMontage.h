// Round Survival

#pragma once

#include "GameplayCueNotify_Static.h"

#include "RSGameplayCueNotify_WeaponMontage.generated.h"

/** Which of the drawn weapon's montage pairs a cue plays. */
UENUM()
enum class ERSWeaponMontageAction : uint8
{
	Fire,
	Reload,
	Holster,
	AimIn,

	/** Added for the strike's length. The raw magnitude carries which strike: 0 and up, or -1 for the lunge. */
	Melee,

	/** Executed where a strike landed, from the cue's location and normal. */
	MeleeImpact
};

/**
 * Plays one of the drawn weapon's montage pairs, on whichever machine the cue reaches.
 *
 * This is how everybody else sees a weapon being used. The abilities that fire, reload and holster
 * run only where the pawn is controlled from and on the server, so a montage they played
 * themselves reached nobody else — and the listen server's own player, whose abilities run on the
 * server alone, was seen by no one at all. Executing a cue instead lets GAS do the routing: a
 * predicting client plays it at once, the server plays it and multicasts it, and the multicast
 * skips the client that has already played it.
 *
 * The cue only says *what* happened. The montages, their pairing, the spare magazine, and for a
 * shot the muzzle flash and the report, all stay on URSWeaponInstance, so a weapon with no reload
 * animation is simply a weapon whose reload cue plays nothing. The play rate travels as the
 * cue's raw magnitude.
 *
 * The fire cue also carries where the shot landed, in its location, normal and physical material,
 * and hands it to URSImpactEffectsSubsystem. One cue a shot for both, rather than a second cue
 * for the impact: at 700 rounds a minute a second multicast per bullet is not free.
 *
 * One class serves every action. Each cue is a Blueprint subclass that sets only its tag and its
 * action, because the cue manager finds notifies by scanning asset paths, not native classes. The
 * draw has no cue: equipping already runs on every machine and plays its own.
 *
 * **Events are executed, states are added.** A shot is an instant, so its cue is executed once. A
 * reload lasts as long as its ability, so the reload ability adds its cue for that long: the cue
 * plays the montages when it goes on and stops them when it comes off, which is what makes a
 * cancelled reload stop on screen on every machine. A holster is a state too, but a switch is never
 * cancelled, so it is still executed; if a switch ever can be, it gets the same treatment.
 */
UCLASS(meta = (DisplayName = "RS Weapon Montage Cue"))
class ROUND_SURVIVAL_API URSGameplayCueNotify_WeaponMontage : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	//~UGameplayCueNotify_Static interface
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	//~End of UGameplayCueNotify_Static interface

protected:
	/** Plays this cue's action on the target's drawn weapon. False when nothing is drawn. */
	bool PlayAction(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon")
	ERSWeaponMontageAction Action = ERSWeaponMontageAction::Fire;
};
