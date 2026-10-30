// Round Survival

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native gameplay tags for Round Survival.
 *
 * Declared here and defined in RSGameplayTags.cpp so that C++ and Blueprint share one
 * source of truth: these exist without anyone having to add them to the tag table by hand,
 * and a typo is a compile error rather than a tag that silently never matches.
 */
namespace RSGameplayTags
{
	// -- Input ---------------------------------------------------------------------------
	// Abilities declare the input tag that activates them; the input config maps input
	// actions to these. Binding a new ability to a key therefore touches no C++.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Jump);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Sprint);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Fire);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Reload);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_ADS);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Interact);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Melee);

	// One per weapon slot plus next / previous. The switch ability is granted once per tag and
	// reads which one it was granted under to know where to go.
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_WeaponSlot_1);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_WeaponSlot_2);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_WeaponSlot_Next);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_WeaponSlot_Previous);

	// -- Abilities -----------------------------------------------------------------------

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Fire);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Reload);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ADS);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Interact);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Revive);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_SwitchWeapon);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Death);

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Zombie_AttackBarricade);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Zombie_TraverseWindow);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Zombie_AttackPlayer);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Zombie_ReachThrough);

	// -- Ability activation failures ------------------------------------------------------
	// Returned to the caller so the HUD can say *why* an ability did not fire.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cost);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cooldown);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_IsDead);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsBlocked);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_NoAmmo);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_NoScrap);

	// -- Character state ------------------------------------------------------------------

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Reloading);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ADS);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Firing);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Melee);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Interacting);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HandsBusy);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Downed);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_GoingDown);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_BeingRevived);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SelfReviving);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_WeaponBlocked);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SwitchingWeapon);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_RegenBlocked);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Crawling);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_TraversingWindow);

	// -- Weapons --------------------------------------------------------------------------
	// Ammo is a tag stack on the weapon item instance, spent by an ability cost. There is
	// deliberately no ammo attribute: a magazine belongs to a weapon, not to a pawn.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Ammo_Magazine);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Ammo_Reserve);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_MagazineSize);

	// -- Damage zones ---------------------------------------------------------------------
	// Carried by the physical materials on the enemy physics asset, not by bone names.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Zone_Head);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Zone_Torso);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Zone_Limb);

	// -- Damage types ---------------------------------------------------------------------
	// Put on the damage effect by the ability that deals it. Its absence means a firearm.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Melee);

	// -- Body parts ------------------------------------------------------------------------
	// Which piece of an enemy came off: the payload of RS.Event.Dismembered, and what an effect
	// that answers one loss rather than another asks for. One per ERSBodyPart.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BodyPart_Head);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BodyPart_ArmLeft);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BodyPart_ArmRight);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BodyPart_LegLeft);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BodyPart_LegRight);

	// -- SetByCaller magnitudes -------------------------------------------------------------

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Healing);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Scrap);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Health);

	// -- Economy --------------------------------------------------------------------------

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Reward_Hit);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Reward_Kill);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Reward_Headshot);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Reward_Repair);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Reward_Hit_Melee);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Reward_Kill_Melee);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Reward_Revive);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Reward_PowerUp);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Spend);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Refund);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Penalty_Downed);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Economy_Penalty_TeammateBledOut);

	// -- Stations ---------------------------------------------------------------------------
	// Granted, for as long as its effect lasts, by the station a player bought. The station
	// definition puts it on the effect spec; the effect asset itself carries no tag of its own.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Station_Vitality);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Station_RapidHands);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Station_Overcharge);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Station_SecondWind);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Station_Endurance);

	// -- Power-ups ---------------------------------------------------------------------------
	// Granted to the whole team, for as long as a timed power-up lasts. Like a station's, the tag
	// rides on the effect spec; the power-up's effect entry names it.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PowerUp_LethalRounds);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PowerUp_Markdown);

	// -- Gameplay events ---------------------------------------------------------------------

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Dismembered);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Melee_Window);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Round_Started);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Round_Ended);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Barricade_SlotGranted);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Barricade_SlotLost);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Barricade_TransitFreed);

	// -- Gameplay cues -----------------------------------------------------------------------
	// Rooted at GameplayCue rather than RS: the cue manager only maps tags under that root to
	// their notifies, so a cue anywhere else would replicate and then find nobody to play it.

	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Fire);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Reload);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Holster);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_AimIn);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_Melee);
	ROUND_SURVIVAL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Weapon_MeleeImpact);
}
