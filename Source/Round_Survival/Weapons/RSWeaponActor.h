// Round Survival

#pragma once

#include "GameFramework/Actor.h"

#include "RSWeaponActor.generated.h"

class UAudioComponent;
class UCameraComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class URSWeaponAssemblyComponent;

/**
 * A weapon in a player's hands: the assembly that shows it, plus the effects of firing it.
 *
 * Purely cosmetic. It holds no ammo, no state and no logic — all of that lives on the replicated
 * `URSEquipmentInstance` that spawned it. Two of these exist per equipped weapon, the viewmodel
 * and the world copy, and they are the same Blueprint: the equipment instance decides which one
 * renders in first person.
 *
 * What the weapon looks like is not this actor's business but its root's: the receiver and its
 * parts are `URSWeaponAssemblyComponent`, which anything else that shows a weapon can carry too.
 * The actor is spawned empty and the weapon instance hands the root the assembly its item names,
 * so one class serves every weapon. What stays here is what only a weapon that is held and fired
 * needs — the muzzle flash and the shot sound. Adding a weapon is a `URSWeaponAssembly` asset named
 * by its item. No C++, and no Blueprint of this class.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	ARSWeaponActor();

	/** The receiver and its parts. Also the root, and the mesh the weapon's montages play on. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	URSWeaponAssemblyComponent* GetAssembly() const { return Assembly; }

	/**
	 * Spawns a one-shot effect at the muzzle, seen by whoever sees this copy of the weapon.
	 *
	 * On the world copy it is simply attached to the muzzle. The viewmodel cannot do that: it is
	 * drawn with the camera's first-person field of view and scale, so it appears somewhere other
	 * than where it is, and Niagara particles do not take that transform -- a flash attached to the
	 * viewmodel muzzle is drawn at the muzzle's real position, off to one side. So for the
	 * viewmodel the effect is placed where the muzzle appears, by applying the renderer's own
	 * first-person transform on the CPU, and attached to the camera.
	 *
	 * Call it on one copy per machine, as URSWeaponInstance::PlayFireEffects does: the effect
	 * carries no owner-only visibility (see ResetMuzzleEffectVisibility).
	 *
	 * With bFollowMuzzle false the effect is left where the muzzle is, or appears to be, instead of
	 * attached to it. That is for smoke on lightweight emitters, which only simulate in their
	 * component's space: on a component that followed the weapon the smoke would be dragged along
	 * with it, and on the viewmodel's, which hangs off the camera, it would stay fixed on screen.
	 */
	UNiagaraComponent* SpawnMuzzleEffect(UNiagaraSystem* System, bool bFollowMuzzle = true) const;

	/** Plays a sound at the muzzle, following the weapon. */
	UAudioComponent* PlayMuzzleSound(USoundBase* Sound) const;

private:
	/**
	 * Where a first-person muzzle appears on screen, relative to the owner's camera. False if the
	 * owner has no camera.
	 */
	bool GetFirstPersonMuzzlePlacement(const FTransform& MuzzleTransform, UCameraComponent*& OutCamera, FTransform& OutRelative) const;

	/** Clears the visibility flags a pooled effect may come back with. */
	static void ResetMuzzleEffectVisibility(UNiagaraComponent* Effect);

	/**
	 * Named ReceiverMesh as a subobject, which is what it was before the assembly had a class of
	 * its own, so the weapon Blueprints keep finding it.
	 */
	UPROPERTY(VisibleAnywhere, Category = "RS|Weapon Art")
	TObjectPtr<URSWeaponAssemblyComponent> Assembly;
};
