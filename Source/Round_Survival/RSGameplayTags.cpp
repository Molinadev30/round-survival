// Round Survival

#include "RSGameplayTags.h"

namespace RSGameplayTags
{
	// -- Input ---------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "RS.InputTag.Move", "Movement axis input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look, "RS.InputTag.Look", "Look axis input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Jump, "RS.InputTag.Jump", "Jump input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Crouch, "RS.InputTag.Crouch", "Crouch input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Sprint, "RS.InputTag.Sprint", "Sprint input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Fire, "RS.InputTag.Fire", "Weapon fire input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Reload, "RS.InputTag.Reload", "Weapon reload input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ADS, "RS.InputTag.ADS", "Aim down sights input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Interact, "RS.InputTag.Interact", "Interact input.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Melee, "RS.InputTag.Melee", "Melee input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_WeaponSlot_1, "RS.InputTag.WeaponSlot.1", "Draw the weapon in the first slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_WeaponSlot_2, "RS.InputTag.WeaponSlot.2", "Draw the weapon in the second slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_WeaponSlot_Next, "RS.InputTag.WeaponSlot.Next", "Draw the next carried weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_WeaponSlot_Previous, "RS.InputTag.WeaponSlot.Previous", "Draw the previous carried weapon.");

	// -- Abilities -----------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fire, "RS.Ability.Fire", "Ranged weapon fire ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Reload, "RS.Ability.Reload", "Weapon reload ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ADS, "RS.Ability.ADS", "Aim down sights ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Sprint, "RS.Ability.Sprint", "Sprint ability, for as long as the input is held.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Interact, "RS.Ability.Interact", "Interaction ability, including holds.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Revive, "RS.Ability.Revive", "Revive a downed player.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_SwitchWeapon, "RS.Ability.SwitchWeapon", "Holster the drawn weapon and draw another carried one.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee, "RS.Ability.Melee", "The player's melee strike.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Death, "RS.Ability.Death", "Death ability, triggered when health reaches zero.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Zombie_AttackBarricade, "RS.Ability.Zombie.AttackBarricade", "Enemy tears a board from a barricade.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Zombie_TraverseWindow, "RS.Ability.Zombie.TraverseWindow", "Enemy vaults through an open barricade.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Zombie_AttackPlayer, "RS.Ability.Zombie.AttackPlayer", "Enemy melee attack on a player.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Zombie_ReachThrough, "RS.Ability.Zombie.ReachThrough", "Enemy at a barricade slot reaches through its gap at a player.");

	// -- Ability activation failures ------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cost, "RS.Ability.ActivateFail.Cost", "Ability failed because its cost could not be paid.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cooldown, "RS.Ability.ActivateFail.Cooldown", "Ability failed because it is on cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_IsDead, "RS.Ability.ActivateFail.IsDead", "Ability failed because the owner is dead.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsBlocked, "RS.Ability.ActivateFail.TagsBlocked", "Ability failed because a blocking tag is present.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_NoAmmo, "RS.Ability.ActivateFail.NoAmmo", "Ability failed because the magazine is empty.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_NoScrap, "RS.Ability.ActivateFail.NoScrap", "Ability failed because the player cannot afford it.");

	// -- Character state ------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "RS.State.Reloading", "Currently reloading.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ADS, "RS.State.ADS", "Currently aiming down sights.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sprinting, "RS.State.Sprinting", "The sprint input is held and the sprint ability is running. Whether the player is actually going faster is the movement component's call.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Firing, "RS.State.Firing", "A shot is under way, cadence wait included. Keeps a held sprint from starting between shots.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Melee, "RS.State.Melee", "A melee strike is under way, recovery included.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Interacting, "RS.State.Interacting", "Currently holding an interaction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HandsBusy, "RS.State.HandsBusy", "Holding an interaction that takes both hands: a revive, a station. Blocks firing and reloading.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Downed, "RS.State.Downed", "Downed and awaiting revive.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_GoingDown, "RS.State.GoingDown", "The moment after going down, while the weapon comes back up. Blocks firing, reloading and aiming.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_BeingRevived, "RS.State.BeingRevived", "Downed, with a teammate holding the revive. Pauses the bleed-out.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SelfReviving, "RS.State.SelfReviving", "Downed with Second Wind in a solo match: gets back up alone instead of bleeding out. Still counts as standing for the match.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "RS.State.Dead", "Dead. Blocks every ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_WeaponBlocked, "RS.State.WeaponBlocked", "Muzzle is against geometry. Reserved for the fallback near-wall behaviour.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SwitchingWeapon, "RS.State.SwitchingWeapon", "Holstering one weapon and drawing another. Blocks firing, reloading and aiming.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_RegenBlocked, "RS.State.RegenBlocked", "Hit recently. Health does not regenerate while this is on.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Crawling, "RS.State.Crawling", "An enemy with a leg gone: it falls and crawls for the rest of its life. Granted by the leg's sever effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_TraversingWindow, "RS.State.TraversingWindow", "An enemy climbing through a window. Anything that would change its body waits for the climb to end.");

	// -- Weapons --------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Ammo_Magazine, "RS.Weapon.Ammo.Magazine", "Rounds currently in the magazine. Tag stack on the weapon instance.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Ammo_Reserve, "RS.Weapon.Ammo.Reserve", "Rounds held in reserve. Tag stack on the weapon instance.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_MagazineSize, "RS.Weapon.MagazineSize", "Magazine capacity. Tag stack on the weapon instance.");

	// -- Damage zones ---------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Zone_Head, "RS.Damage.Zone.Head", "Head hit zone. Carried by the physical material.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Zone_Torso, "RS.Damage.Zone.Torso", "Torso hit zone. Carried by the physical material.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Zone_Limb, "RS.Damage.Zone.Limb", "Limb hit zone. Carried by the physical material.");

	// -- Damage types ---------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Melee, "RS.Damage.Type.Melee", "Damage dealt by a melee attack. Absent means a firearm.");

	// -- Body parts ------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BodyPart_Head, "RS.BodyPart.Head", "An enemy's head, as a part that can come off.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BodyPart_ArmLeft, "RS.BodyPart.ArmLeft", "An enemy's left arm, from the shoulder.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BodyPart_ArmRight, "RS.BodyPart.ArmRight", "An enemy's right arm, from the shoulder.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BodyPart_LegLeft, "RS.BodyPart.LegLeft", "An enemy's left leg, from the hip.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BodyPart_LegRight, "RS.BodyPart.LegRight", "An enemy's right leg, from the hip.");

	// -- SetByCaller magnitudes -------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "RS.SetByCaller.Damage", "Damage magnitude passed into a gameplay effect spec.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Healing, "RS.SetByCaller.Healing", "Healing magnitude passed into a gameplay effect spec.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Scrap, "RS.SetByCaller.Scrap", "Scrap magnitude passed into a gameplay effect spec.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Health, "RS.SetByCaller.Health", "Health magnitude passed into a gameplay effect spec. The round director scales enemies with it.");

	// -- Economy --------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Reward_Hit, "RS.Economy.Reward.Hit", "Scrap awarded for damaging an enemy.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Reward_Kill, "RS.Economy.Reward.Kill", "Scrap awarded for killing an enemy.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Reward_Headshot, "RS.Economy.Reward.Headshot", "Scrap awarded for a headshot kill.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Reward_Repair, "RS.Economy.Reward.Repair", "Scrap awarded for restoring a barricade board.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Reward_Hit_Melee, "RS.Economy.Reward.Hit.Melee", "Scrap awarded for a melee hit on an enemy.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Reward_Kill_Melee, "RS.Economy.Reward.Kill.Melee", "Scrap awarded for a melee kill.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Reward_Revive, "RS.Economy.Reward.Revive", "Scrap paid to a reviver: what the revived player lost going down.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Spend, "RS.Economy.Spend", "Scrap spent on a purchase.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Refund, "RS.Economy.Refund", "Scrap handed back for a purchase that gave nothing, such as a Supply Cache roll that ended with the cache moving.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Penalty_Downed, "RS.Economy.Penalty.Downed", "Scrap lost going down. Paid to whoever revives.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Reward_PowerUp, "RS.Economy.Reward.PowerUp", "Scrap a power-up paid to every player: Purge, Fortify.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Economy_Penalty_TeammateBledOut, "RS.Economy.Penalty.TeammateBledOut", "Scrap lost by every living player when a teammate bleeds out.");

	// -- Stations ---------------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Station_Vitality, "RS.Station.Vitality", "Has the Vitality station: a higher maximum health.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Station_RapidHands, "RS.Station.RapidHands", "Has the Rapid Hands station: faster reloads.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Station_Overcharge, "RS.Station.Overcharge", "Has the Overcharge station: faster and harder-hitting fire.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Station_SecondWind, "RS.Station.SecondWind", "Has the Second Wind station: alone in the match, gets back up alone once downed; with teammates, revives them in half the time.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Station_Endurance, "RS.Station.Endurance", "Has the Endurance station: a sprint that lasts twice as long, and slightly faster movement.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PowerUp_LethalRounds, "RS.PowerUp.LethalRounds", "Lethal Rounds is on: any damage this player deals to an enemy kills it.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PowerUp_Markdown, "RS.PowerUp.Markdown", "Markdown is on: the Supply Cache charges this player its markdown price.");

	// -- Gameplay events ---------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "RS.Event.Death", "Sent when an actor's health reaches zero.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Dismembered, "RS.Event.Dismembered", "Sent to an enemy when a part comes off. The target tags carry the RS.BodyPart.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Melee_Window, "RS.Event.Melee.Window", "Sent by the melee window notify state. The magnitude carries the window's duration in seconds.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Round_Started, "RS.Event.Round.Started", "Sent when a round begins.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Round_Ended, "RS.Event.Round.Ended", "Sent when a round is cleared.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Barricade_SlotGranted, "RS.Event.Barricade.SlotGranted", "StateTree event: this enemy now holds a place at its window.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Barricade_SlotLost, "RS.Event.Barricade.SlotLost", "StateTree event: this enemy no longer holds a place at its window.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Barricade_TransitFreed, "RS.Event.Barricade.TransitFreed", "StateTree event: the crossing at this enemy's window is no longer held by anyone else.");

	// -- Gameplay cues -----------------------------------------------------------------------

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Fire, "GameplayCue.Weapon.Fire", "The drawn weapon's fire montage pair and where the shot landed. The raw magnitude carries the play rate; location, normal and physical material the impact, with a zero normal for a shot that hit nothing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Reload, "GameplayCue.Weapon.Reload", "The drawn weapon's reload montage pair. The raw magnitude carries the play rate.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Holster, "GameplayCue.Weapon.Holster", "The drawn weapon's holster montage pair. The raw magnitude carries the play rate.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_AimIn, "GameplayCue.Weapon.AimIn", "The drawn weapon's aim-in sound, once each time its sights come up.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_Melee, "GameplayCue.Weapon.Melee", "A melee strike, for its length. The raw magnitude carries which strike; -1 is the lunge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Weapon_MeleeImpact, "GameplayCue.Weapon.MeleeImpact", "Where a melee strike landed on an enemy.");
}
