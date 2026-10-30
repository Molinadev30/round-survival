// Round Survival

#pragma once

#include "Equipment/RSEquipmentInstance.h"
#include "GameplayTagContainer.h"
#include "Weapons/RSFirstPersonRecoilComponent.h"
#include "Weapons/RSLeftHandGrip.h"

#include "RSWeaponInstance.generated.h"

class UAnimInstance;
class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class USkeletalMeshComponent;
class URSMeleeStrikeSet;
class URSWeaponAssembly;
class URSWeaponAssemblyComponent;
class URSWeaponItemInstance;

/**
 * What every weapon has, regardless of how it deals damage.
 *
 * Only timing and animation assets live here. Ammo does not: it is a tag stack on the weapon item
 * this was drawn from — the instigator — because this instance is destroyed on every holster and
 * the item is not.
 *
 * **Drawing plays itself.** OnEquipped plays the equip pair on every machine that equips this, so
 * the draw needs no ability and looks the same whether it came from a switch, a purchase or a
 * respawn. The holster pair is the switch ability's to ask for, because only it knows a holster is
 * coming.
 *
 * **Every other pair is played by a gameplay cue, never by an ability directly.** The abilities
 * run only where the pawn is controlled from and on the server, so a montage they played
 * themselves was seen by nobody else. They execute a `GameplayCue.Weapon.*` cue instead, and
 * `URSGameplayCueNotify_WeaponMontage` calls the Play functions below on every machine it reaches.
 * The abilities take their timing from the Get*Duration functions, which read the assets and so
 * give the same answer whether or not anything is played on that machine.
 *
 * **How a weapon brings its own animation.** The character's animation Blueprints know nothing
 * about rifles. Each weapon names an anim layer class per perspective, and equipping links that
 * layer into the character's graph; unequipping unlinks it. The weapon mesh has its own skeleton
 * and its own animations, authored frame for frame against the character's, so it is animated in
 * parallel rather than parented to a hand — which is why every action plays a *pair* of montages,
 * one on the character and one on the weapon.
 *
 * Adding a weapon is therefore data plus its anim layer Blueprints. No C++ per weapon.
 *
 * **Where the left hand goes.** The weapon's parts decide the grip (see FRSWeaponPart::LeftHandGrip):
 * a vertical foregrip means the vertical hold, an angled one the angled hold, none the handguard.
 * This instance owns the poses for each grip and hands the right ones to both character meshes
 * when it is drawn.
 */
UCLASS()
class ROUND_SURVIVAL_API URSWeaponInstance : public URSEquipmentInstance
{
	GENERATED_BODY()

public:
	URSWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~URSEquipmentInstance interface
	virtual void OnEquipped() override;
	virtual void OnUnequipped() override;
	virtual void OnInstigatorChanged() override;
	//~End of URSEquipmentInstance interface

	/** The weapon item this was drawn from, where its ammo lives. Null until it has replicated. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	URSWeaponItemInstance* GetItem() const;

	/** A stack on the item, or zero while the item is not known on this machine yet. */
	int32 GetItemStatTagStackCount(FGameplayTag Tag) const;

	/** Called by the fire ability. Drives spread recovery and the "recently fired" pose. */
	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	void UpdateFiringTime();

	/**
	 * Seconds since this weapon was last fired or equipped.
	 *
	 * Equipping counts as an interaction so a weapon just pulled out does not immediately drop
	 * into an idle-for-a-long-time pose.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	float GetTimeSinceLastInteractedWith() const;

	/**
	 * Plays the fire pair.
	 *
	 * The first-person montage plays on the viewmodel and the third-person one on the body, so the
	 * shooter and everyone else see their own version of the same shot. Each machine plays what it
	 * can see: a montage is cosmetic and never gates the shot itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	void PlayFireMontages(float PlayRate = 1.0f);

	/**
	 * Everything one shot looks and sounds like: the fire pair, the muzzle flash and the report.
	 *
	 * The fire cue's entry point, so it runs on every machine the shot reaches. On the shooter's own
	 * machine it also kicks the viewmodel (see FRSWeaponRecoil). Flash and sound go
	 * on one copy of the weapon per machine: the viewmodel where there is one -- the shooter's own
	 * machine -- and the world copy everywhere else, so nobody sees or hears a shot twice and
	 * everybody sees and hears it.
	 */
	void PlayFireEffects(float PlayRate = 1.0f);

	/** The aim-in cue's entry point: AimInSound, on the copy on screen here, as a shot's report is. */
	void PlayAimInSound();

	/**
	 * Plays the reload pair, and fills the spare magazine for it.
	 *
	 * Which magazine is visible when is not decided here: the weapon montages carry
	 * `URSAnimNotifyState_WeaponPartVisibility` windows, so the parts swap in step with the bones
	 * that move them.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	void PlayReloadMontages(float PlayRate = 1.0f);

	/**
	 * Stops the reload pair, with each montage's own blend out, where it is still playing. Run when
	 * the reload cue comes off, so a reload cut short stops on screen too; the part visibility
	 * windows close with their montage.
	 */
	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	void StopReloadMontages();

	/** Plays the holster pair. */
	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	void PlayHolsterMontages(float PlayRate = 1.0f);

	/**
	 * The melee cue's entry point: one strike from MeleeStrikes — its pair at the set's play rate, and
	 * the whoosh at its moment, on the copy on screen here as a shot's report is.
	 * @param Variant  0 and up for the alternating strikes, INDEX_NONE for the lunge.
	 */
	void PlayMeleeStrike(int32 Variant);

	/** Stops whatever strike is still playing, and its whoosh if it has not been heard yet. Run when the melee cue comes off. */
	void StopMeleeStrike();

	/** The melee impact cue's entry point: the impact sound and effect where a strike landed. */
	void PlayMeleeImpact(const FVector& Location, const FVector& Normal) const;

	/** How this weapon is swung. Null for one that has no strike of its own; the melee ability then uses its own timing. */
	const class URSMeleeStrikeSet* GetMeleeStrikes() const { return MeleeStrikes; }

	/** Where the left hand holds this weapon, from its item's assembly. Handguard while the item is unknown. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	ERSLeftHandGrip GetLeftHandGrip() const;

	/**
	 * Seconds a pair lasts on screen at a play rate of one; zero for a weapon without that pair.
	 *
	 * Readable on the class default object, which is how the switch ability knows how long the draw
	 * of a weapon that has not been created yet will take.
	 */
	float GetEquipDuration() const;
	float GetHolsterDuration() const;
	float GetReloadDuration() const;

	/** How far through the reload the rounds move, from 0 to 1. See ReloadAmmoTransferFraction. */
	float GetReloadAmmoTransferFraction() const { return ReloadAmmoTransferFraction; }

	UAnimMontage* GetFirstPersonFireMontage() const { return FirstPersonFireMontage; }
	UAnimMontage* GetThirdPersonFireMontage() const { return ThirdPersonFireMontage; }
	UAnimMontage* GetFirstPersonReloadMontage() const { return FirstPersonReloadMontage; }
	UAnimMontage* GetThirdPersonReloadMontage() const { return ThirdPersonReloadMontage; }

protected:
	// -- Anim layers -----------------------------------------------------------------------------

	/** Linked into the viewmodel's graph while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TSubclassOf<UAnimInstance> FirstPersonAnimLayer;

	/** Linked into the body's graph while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TSubclassOf<UAnimInstance> ThirdPersonAnimLayer;

	/** Runs on the weapon's own mesh, on the weapon's own skeleton. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TSubclassOf<UAnimInstance> WeaponAnimClass;

	// -- Montages ---------------------------------------------------------------------------------
	// Character montages move the arms and body; the weapon montages move the weapon itself. They
	// are authored as pairs and have to be played as pairs.

	/** Played on the viewmodel; only the owning player ever sees it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> FirstPersonFireMontage;

	/** Played on the body mesh; what other players see. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> ThirdPersonFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> FirstPersonReloadMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> ThirdPersonReloadMontage;

	/** The weapon's half of the fire pair, played on the viewmodel weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> FirstPersonWeaponFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> ThirdPersonWeaponFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> FirstPersonWeaponReloadMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> ThirdPersonWeaponReloadMontage;

	/** Bringing the weapon up. Played by OnEquipped on every machine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> FirstPersonEquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> ThirdPersonEquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> FirstPersonWeaponEquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> ThirdPersonWeaponEquipMontage;

	/** Putting the weapon away. Played by the switch ability before the swap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> FirstPersonHolsterMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> ThirdPersonHolsterMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> FirstPersonWeaponHolsterMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<UAnimMontage> ThirdPersonWeaponHolsterMontage;

	/**
	 * The melee strikes, as data shared by every weapon held this way. The character's half only: the
	 * weapon is carried in the hands through a strike and has no montage of its own.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TObjectPtr<URSMeleeStrikeSet> MeleeStrikes;

	// -- Left-hand grip -----------------------------------------------------------------------------

	/**
	 * The poses for each grip this weapon can be held by. The handguard needs none: it is the
	 * hold the animations are authored with. A grip the parts declare but that has no entry here is
	 * held like the handguard.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	TMap<ERSLeftHandGrip, FRSLeftHandGripPoses> LeftHandGripPoses;

	// -- Fire effects -----------------------------------------------------------------------------
	// Data rather than notifies in the fire montage: a notify only fires where its montage plays,
	// and the weapon's own fire montage plays only on the viewmodel, which only the shooter has.

	/** Spawned at the muzzle on every shot, and follows it: the flame and its light. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fire Effects")
	TObjectPtr<UNiagaraSystem> MuzzleFlash;

	/**
	 * Spawned where the muzzle is on every shot and left there, so it hangs in the air as the weapon
	 * moves on. A system of its own because a lightweight emitter simulates in its component's
	 * space, and the flash's component follows the muzzle.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fire Effects")
	TObjectPtr<UNiagaraSystem> MuzzleSmoke;

	/** Played together at the muzzle on every shot: the report and, layered on it, its tail. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fire Effects")
	TArray<TObjectPtr<USoundBase>> FireSounds;

	/** The kick each shot gives the viewmodel, on the shooter's screen only. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fire Effects")
	FRSWeaponRecoil Recoil;

	/** The foley of the sights coming up, each time they do. None for a weapon that is not aimed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Fire Effects")
	TObjectPtr<USoundBase> AimInSound;

	// -- Magazine display -------------------------------------------------------------------------

	/** The weapon assembly part that shows the rounds in the magazine. None for a weapon without one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	FName LoadedMagazinePart;

	/** The part the reload brings in, filled when the reload starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Animation")
	FName SpareMagazinePart;

	/**
	 * Where in the reload the rounds actually move, as a fraction of its duration.
	 *
	 * Not the end, when the weapon has a visible magazine swap. The loaded magazine reappears in
	 * the well on the reload's last frame, so its new count has to be in place before then — and
	 * after it has been hidden, or the old magazine would refill in the player's hand. For the
	 * rifle both hide windows open before 0.69 and the unlock authored in the montage sits at 0.72.
	 * Cancelling after this point keeps the rounds, as it does in the genre.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Weapon", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float ReloadAmmoTransferFraction = 1.0f;

private:
	/** Poses the loaded magazine on both weapon copies to the current count. */
	void RefreshLoadedMagazine();

	/**
	 * Fills the spare magazine to what the loaded one will hold once this reload lands.
	 *
	 * Set once, when the reload starts, and not tied to the ammo counts afterwards: the spare
	 * stays on screen after the rounds move, and following the reserve count would visibly empty
	 * it in the player's hand. Read from the item here rather than handed in by the reload ability,
	 * so a machine that only receives the cue fills it just the same.
	 */
	void ShowSpareMagazineForReload();

	/** Starts following the item's ammo, once, whichever of OnEquipped or the instigator gets here first. */
	void BindToItem();
	void UnbindFromItem();
	void HandleItemStatsChanged(URSWeaponItemInstance* Item);
	void HandleItemSkinChanged(URSWeaponItemInstance* Item);

	/** Hands both character meshes the poses for this weapon's grip. */
	void ApplyLeftHandGrip();

	/** Takes the grip poses back off both meshes, so the next weapon starts from its own hold. */
	void ClearLeftHandGrip();

	/** Hands the viewmodel this weapon's sight offset. */
	void ApplyAimDownSightsOffset();

	/**
	 * Builds the item's assembly on both weapon copies. They are spawned empty, so until the item is
	 * known on this machine there is no weapon to see rather than a wrong one.
	 */
	void RefreshAssembly();

	/** The assembly the item names, or null while the item is unknown. */
	const URSWeaponAssembly* GetItemAssembly() const;

	/** Puts the item's skin on both weapon copies, or the mesh's own finish while the item is unknown. */
	void RefreshSkin();

	/** The longer of a pair's on-screen lengths, ignoring play rate. */
	static float GetPairLength(const UAnimMontage* FirstPersonMontage, const UAnimMontage* ThirdPersonMontage);

	/** Plays a first-person / third-person pair on the character. */
	void PlayMontagePair(UAnimMontage* FirstPersonMontage, UAnimMontage* ThirdPersonMontage, float PlayRate);

	/** The same, on the spawned weapon actors rather than on the character. */
	void PlayWeaponMontagePair(UAnimMontage* FirstPersonMontage, UAnimMontage* ThirdPersonMontage, float PlayRate);

	/** Stops a pair on the character, if it is playing. */
	void StopMontagePair(const UAnimMontage* FirstPersonMontage, const UAnimMontage* ThirdPersonMontage);

	/** The same, on the spawned weapon actors. */
	void StopWeaponMontagePair(const UAnimMontage* FirstPersonMontage, const UAnimMontage* ThirdPersonMontage);

	/** Links this weapon's layers into the character graphs and drives the weapon meshes. */
	void ApplyAnimLayers();
	void ClearAnimLayers();

	/** The skeletal mesh of the spawned viewmodel or world weapon actor. */
	USkeletalMeshComponent* GetWeaponMeshComponent(bool bFirstPerson) const;

	/** The receiver and parts of the spawned viewmodel or world weapon actor, or null. */
	URSWeaponAssemblyComponent* GetWeaponAssembly(bool bFirstPerson) const;

	double TimeLastEquipped = 0.0;
	double TimeLastFired = 0.0;

	/** Waits for the strike's whoosh. */
	FTimerHandle MeleeWhooshTimerHandle;

	/** The item whose OnStatsChanged this is bound to, so the binding is undone on the same object. */
	TWeakObjectPtr<URSWeaponItemInstance> BoundItem;
	FDelegateHandle ItemStatsChangedHandle;
	FDelegateHandle ItemSkinChangedHandle;
};
