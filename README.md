# Round Survival

Round-based co-op horde survival prototype in **Unreal Engine 5.8**, built as a gameplay engineering portfolio piece. This repository holds the C++ gameplay code. The architecture is the point of it, not the art.

> **Source only.** The game cannot be played from this repository. Maps, Blueprints, data assets, StateTree assets and animation graphs live in `Content/`, which is not published because it depends on licensed third-party assets whose terms do not allow redistribution. The code compiles on its own; it has nothing to run on without that content.

## What is in here

- **Server-authoritative from the start.** No purchase, damage, reward or round advance executes on a client. Clients predict presentation and local targeting; the server validates and applies. The target is a listen server, and Standalone runs the same authority path.
- **Gameplay Ability System as the combat layer.** Health, damage, the scrap economy, round scaling, buffs and status all flow through attributes, gameplay effects, tags and abilities. Ammo is a tag stack on the weapon item, spent by an ability cost.
- **Item and equipment split.** What a player carries (`URSWeaponItemDefinition`, `URSWeaponItemInstance`, `URSWeaponSlotsComponent`) is separate from how it is equipped (`URSEquipmentDefinition`, `URSEquipmentInstance`, `URSEquipmentManagerComponent`). Drawing a weapon creates its equipment instance and grants its ability sets; holstering destroys it, and the item survives.
- **StateTree enemy AI with a C++ round director.** Heavy AI work lives in C++ StateTree tasks, and the round flow stays a plain, readable state machine.
- **64 concurrent enemies as a constraint.** Minimal ASC replication on enemies, unpredicted server-only enemy abilities, staggered path queries, update rate optimisation on meshes and a Significance Manager.
- **Data-driven tuning.** Round curves, costs, rewards, weapon handling and enemy stats belong to data assets, not to code.
- **No polling on tick.** The HUD updates from delegates and gameplay events, and AI targeting and interaction traces run on staggered timers.

## Layout

```
Source/Round_Survival/
  AbilitySystem/   Attribute sets, damage pipeline, ability sets, base abilities and costs
  Animation/       Anim instances and notifies
  Camera/          Player camera manager and local player
  Core/            Game mode, game state, player character, movement component
  Debug/           Console commands and debug subsystem
  Enemies/         Zombie character, enemy abilities, StateTree tasks and conditions
  Equipment/       Equipment definitions, instances and manager
  Input/           Enhanced Input configuration
  Interaction/     Interactables: barricades, gates, lockers, stations
  Performance/     Significance Manager
  Physics/         Physical materials with gameplay tags
  PowerUps/        Power-ups and their effects
  Rounds/          Round director, spawning, zones
  UI/              HUD subsystem, widgets and viewmodels
  Weapons/         Weapon items, firing, reloading, aiming, impacts

Plugins/AnimNodes/ Custom anim graph nodes: bone twist distribution, procedural aim offset
```

## Building

Requires Unreal Engine 5.8 and Visual Studio with the C++ game development workload. Right-click `Round_Survival.uproject`, choose *Generate Visual Studio project files*, and build the `Development Editor` configuration.

## Licence

All rights reserved. The code is published to be read; no licence to use, copy or modify it is granted.
