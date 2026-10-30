// Round Survival

#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "Weapons/RSLeftHandGrip.h"

#include "RSWeaponAssemblyComponent.generated.h"

struct FRSWeaponPart;
class URSWeaponAssembly;
class URSWeaponSkin;

/**
 * Builds a weapon's look: its receiver, which is this component's own mesh, plus the parts bolted
 * onto it, from a `URSWeaponAssembly`.
 *
 * Owns the look and nothing else — no ammo, no firing, no state — and holds no data of its own:
 * whoever shows the weapon hands it an assembly. `ARSWeaponActor` has one as its root for the weapon
 * in a player's hands, and anything that only displays a weapon carries one the same way, so a
 * weapon is assembled by one piece of code wherever it appears.
 *
 * The parts are components of the owning actor, hung on this one. They are derived: never saved
 * with a level nor copied into PIE, because the owner builds them again on construction and on
 * BeginPlay.
 *
 * **A part behaves the way its receiver does**: when it is built it takes the receiver's owner-only
 * and first-person flags, so the parts of a viewmodel render as a viewmodel whether the flags were
 * set before the assembly arrived or after, and whether the receiver ticks and updates its bones,
 * so a weapon that only hangs on a wall is still all the way through while one in a player's hands
 * animates entirely.
 *
 * A part hidden by default is hidden in game, where something may show it later. Outside a game
 * world — a weapon on a locker in the level editor — nothing ever will, so it is not drawn at all,
 * rather than showing the spare magazine parked below the weapon.
 */
UCLASS(ClassGroup = "RoundSurvival", meta = (BlueprintSpawnableComponent))
class ROUND_SURVIVAL_API URSWeaponAssemblyComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	URSWeaponAssemblyComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Puts on the receiver and builds the parts, tearing down what the previous assembly built.
	 * Null leaves the component empty, with no mesh. Rebuilds even when given the same assembly, so
	 * an owner's construction can call it on every editor edit. Does nothing on a component with no
	 * owner, such as a class default object's template, beyond remembering the assembly.
	 */
	void SetAssembly(const URSWeaponAssembly* InAssembly);

	const URSWeaponAssembly* GetAssembly() const { return Assembly; }

	/** The component built for a named part, or null. The reload cue finds the magazine this way. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	USceneComponent* GetPartComponent(FName PartName) const;

	/** Shows or hides one part by name, with everything hung on it. Does nothing if there is no such part. */
	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	void SetPartHidden(FName PartName, bool bInHidden);

	/** Poses a part's FillAnimation to show Rounds out of Capacity. Does nothing without one. */
	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	void SetPartRoundCount(FName PartName, int32 Rounds, int32 Capacity);

	/** Repaints the receiver and every part that accepts a skin; null puts the mesh's own materials back. */
	void ApplySkin(const URSWeaponSkin* Skin);

	/**
	 * The component the muzzle sits on, with its socket and the offset from it. See
	 * FRSWeaponPart::MuzzleSocket. Falls back to the receiver's own muzzle socket, and is null
	 * while there is no assembly.
	 */
	USceneComponent* FindMuzzle(FName& OutSocket, FVector& OutOffset) const;

	/**
	 * The box around the receiver and every part shown by default, in this component's own space.
	 * Invalid while there is no assembly. The meshes' own bounds in their reference pose, so it does
	 * not depend on what anything is animating.
	 */
	FBox GetAssembledLocalBounds() const;

	/**
	 * Hang the assembled weapon by its middle rather than by its origin: after every rebuild the
	 * component moves so that the centre of `GetAssembledLocalBounds` sits on its attach parent.
	 *
	 * **A weapon's origin is its pistol grip**, where the hand holds it, and how far the rest of
	 * it reaches from there differs per weapon — a silencer, a longer handguard, a scope. Something
	 * that only displays a weapon wants it centred on a spot, and that spot is the attach parent:
	 * the display places the parent, the weapon centres itself on it, and no placement has to know
	 * which weapon it holds. Off for a weapon in a player's hands, which hangs by its grip.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon")
	bool bCentreOnAttachParent = false;

private:
	/** Moves the component so the assembled weapon's centre sits on its attach parent. See bCentreOnAttachParent. */
	void CentreOnAttachParent();

	void RebuildParts();
	void DestroyParts();
	void BuildRepeatedMeshes(const FRSWeaponPart& Part, USceneComponent* PartComponent);

	/**
	 * Gives a freshly built part the receiver's view flags and, for a skeletal part, its tick and
	 * bone-update settings. Called before the part registers. See the class comment.
	 */
	void CopyReceiverSettingsTo(UPrimitiveComponent* Part) const;

	/** Hides a part that is hidden by default: in game only, or entirely outside a game world. */
	void ApplyDefaultHidden(USceneComponent* Part, bool bHiddenByDefault) const;

	UPROPERTY(Transient)
	TObjectPtr<const URSWeaponAssembly> Assembly;

	/** Built from the assembly's parts, torn down and rebuilt whenever the assembly is set. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USceneComponent>> PartComponents;

	/** The RepeatedMesh copies, which have no name of their own to be found by. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> RepeatedComponents;
};
