// Round Survival

#include "Weapons/RSGameplayCueNotify_WeaponMontage.h"

#include "Equipment/RSEquipmentManagerComponent.h"
#include "Equipment/RSWeaponInstance.h"
#include "GameFramework/Actor.h"
#include "Weapons/RSImpactEffectsSubsystem.h"

namespace
{
	/**
	 * The target is the avatar. The weapon is looked up on arrival rather than carried in the cue,
	 * because equipment already replicates to every machine; a cue that arrives with nothing drawn
	 * plays nothing.
	 */
	URSWeaponInstance* FindDrawnWeapon(AActor* Target)
	{
		const URSEquipmentManagerComponent* EquipmentManager = Target ? Target->FindComponentByClass<URSEquipmentManagerComponent>() : nullptr;
		return EquipmentManager ? EquipmentManager->GetFirstInstanceOfType<URSWeaponInstance>() : nullptr;
	}
}

bool URSGameplayCueNotify_WeaponMontage::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// Before the weapon is looked for: where the bullet landed does not depend on what is in the
	// shooter's hands by the time the cue arrives. A zero normal is a shot that hit nothing.
	if (Action == ERSWeaponMontageAction::Fire && !Parameters.Normal.IsNearlyZero())
	{
		if (URSImpactEffectsSubsystem* Impacts = URSImpactEffectsSubsystem::Get(MyTarget))
		{
			Impacts->PlayImpact(Parameters.Location, Parameters.Normal, Parameters.PhysicalMaterial.Get());
		}
	}

	return PlayAction(MyTarget, Parameters);
}

bool URSGameplayCueNotify_WeaponMontage::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// Added rather than executed: the reload, for as long as its ability runs.
	return PlayAction(MyTarget, Parameters);
}

bool URSGameplayCueNotify_WeaponMontage::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	URSWeaponInstance* Weapon = FindDrawnWeapon(MyTarget);
	if (!Weapon)
	{
		return false;
	}

	// Only the reload and the strike are added. One that ran to the end has already finished its
	// montages, so this stops nothing; one cut short stops where it is.
	if (Action == ERSWeaponMontageAction::Reload)
	{
		Weapon->StopReloadMontages();
	}
	else if (Action == ERSWeaponMontageAction::Melee)
	{
		Weapon->StopMeleeStrike();
	}

	return true;
}

bool URSGameplayCueNotify_WeaponMontage::PlayAction(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	URSWeaponInstance* Weapon = FindDrawnWeapon(MyTarget);
	if (!Weapon)
	{
		return false;
	}

	// Zero when the sender set none, which the weapon reads as a play rate of one. The melee cues use
	// the magnitude for which strike instead; their rate is the strike set's.
	const float PlayRate = Parameters.RawMagnitude;

	switch (Action)
	{
	case ERSWeaponMontageAction::Fire:
		Weapon->PlayFireEffects(PlayRate);
		break;

	case ERSWeaponMontageAction::Reload:
		Weapon->PlayReloadMontages(PlayRate);
		break;

	case ERSWeaponMontageAction::Holster:
		Weapon->PlayHolsterMontages(PlayRate);
		break;

	case ERSWeaponMontageAction::AimIn:
		Weapon->PlayAimInSound();
		break;

	case ERSWeaponMontageAction::Melee:
		Weapon->PlayMeleeStrike(FMath::RoundToInt(Parameters.RawMagnitude));
		break;

	case ERSWeaponMontageAction::MeleeImpact:
		Weapon->PlayMeleeImpact(Parameters.Location, Parameters.Normal);
		break;
	}

	return true;
}
