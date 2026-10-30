// Round Survival

#pragma once

#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "Weapons/RSLeftHandGrip.h"

#include "RSWeaponAssembly.generated.h"

class UAnimInstance;
class UAnimSequenceBase;
class USkeletalMesh;
class UStaticMesh;

/**
 * One piece bolted onto a weapon: a handguard, a magazine, a sight, the round in the chamber.
 *
 * Weapons in this project are assemblies, not single meshes, because the animations author
 * them that way — the magazine is its own skeletal mesh with its own animation, and it has to be
 * able to drop out of the receiver during a reload. A part therefore names the socket it hangs
 * from rather than carrying a hand-tuned offset.
 */
USTRUCT(BlueprintType)
struct FRSWeaponPart
{
	GENERATED_BODY()

	/** Identifies the part so another part can hang off it. Also names the component. */
	UPROPERTY(EditAnywhere, Category = "Part")
	FName PartName;

	/** Use one or the other. A skeletal part is for anything that animates, such as a magazine. */
	UPROPERTY(EditAnywhere, Category = "Part")
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, Category = "Part")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Only meaningful for a skeletal part. */
	UPROPERTY(EditAnywhere, Category = "Part")
	TSubclassOf<UAnimInstance> AnimClass;

	/** Socket on whatever this attaches to. */
	UPROPERTY(EditAnywhere, Category = "Part")
	FName AttachSocket;

	/** Another part's name, or empty to hang off the receiver. */
	UPROPERTY(EditAnywhere, Category = "Part")
	FName AttachToPart;

	/**
	 * Placed once on every socket or bone of this part whose name starts with RepeatedSocketPrefix.
	 *
	 * The magazine is the case: its skeleton has a bone per round but its mesh has no rounds in it,
	 * so a transparent magazine is empty until a round is hung on each bullet bone.
	 */
	UPROPERTY(EditAnywhere, Category = "Part")
	TObjectPtr<UStaticMesh> RepeatedMesh;

	UPROPERTY(EditAnywhere, Category = "Part")
	FString RepeatedSocketPrefix;

	/**
	 * Posed at a fixed frame instead of played, so the part shows how full it is.
	 *
	 * Frame 0 is full and the last frame is empty. The magazine's depletion animation lifts the
	 * follower and scales spent rounds to zero, and anything hung on those bones follows. Only
	 * meaningful for a skeletal part, and replaces AnimClass when set.
	 */
	UPROPERTY(EditAnywhere, Category = "Part")
	TObjectPtr<UAnimSequenceBase> FillAnimation;

	/**
	 * Built but not drawn until something asks for it.
	 *
	 * The spare magazine is the case: the reload animation parks it below the weapon and only
	 * brings it up mid-reload, so it has to exist the whole time and be invisible most of it.
	 */
	UPROPERTY(EditAnywhere, Category = "Part")
	bool bHiddenByDefault = false;

	/**
	 * Whether a weapon skin repaints this part. Off for the rounds: a camouflaged bullet in the
	 * chamber reads as a bug, not a finish. The RepeatedMesh copies are never skinned.
	 */
	UPROPERTY(EditAnywhere, Category = "Part")
	bool bAcceptsSkin = true;

	/**
	 * Where the muzzle is when this part is fitted: a socket on this part, plus an offset from it.
	 *
	 * The muzzle belongs to whatever part ends the barrel, not to the receiver: each handguard puts
	 * the end of the barrel somewhere else, and a silencer moves it again. The last part in the
	 * list that names a socket or an offset wins, so a silencer declared after the handguard takes
	 * over from it. Leave both empty on a part that has nothing to do with the muzzle.
	 */
	UPROPERTY(EditAnywhere, Category = "Part|Muzzle")
	FName MuzzleSocket;

	/** From MuzzleSocket, or from the part's origin when there is no socket — a silencer has none. */
	UPROPERTY(EditAnywhere, Category = "Part|Muzzle")
	FVector MuzzleOffset = FVector::ZeroVector;

	/**
	 * Where this part puts the left hand: Vertical on a vertical foregrip, Angled on an angled one.
	 * Leave it on Handguard for every other part. The weapon is held by the last part in the list
	 * that sets something else, the same rule as the muzzle, and by the handguard if none does.
	 */
	UPROPERTY(EditAnywhere, Category = "Part|Grip")
	ERSLeftHandGrip LeftHandGrip = ERSLeftHandGrip::Handguard;
};

/**
 * What a weapon looks like: a receiver and the parts bolted onto it.
 *
 * Data only. `URSWeaponAssemblyComponent` builds it wherever a weapon is shown — in a player's
 * hands, on a Wall Locker. It is named by the weapon item, next to the item's skin and for the same
 * reason: the look is part of what tells two weapons apart that are drawn and handled the same way.
 * A separate asset rather than a struct on the item, so that one copy of a weapon can wear another
 * assembly by reference, the way it can already wear another skin.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSWeaponAssembly : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The animated body of the weapon. Everything else hangs off its sockets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Assembly")
	TObjectPtr<USkeletalMesh> ReceiverMesh;

	/** In the order they are bolted on: a part can hang off one listed before it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Assembly", meta = (TitleProperty = PartName))
	TArray<FRSWeaponPart> Parts;

	/** The muzzle on the receiver, used when no part declares one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Assembly")
	FName ReceiverMuzzleSocket = TEXT("SOCKET_Muzzle");

	/**
	 * Where the weapon moves while aiming down sights, so this assembly's sight lines up with the eye.
	 *
	 * The aimed animations bring the weapon up to the face for one sight; a scope sits higher than
	 * iron sights, and a different handguard moves the front post. Applied to the viewmodel's weapon
	 * bone along the mesh's own axes — which follow the camera — and faded in with the aim. Measured
	 * against the view in the aimed pose, not eyeballed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Assembly|Aiming")
	FTransform AimDownSightsOffset = FTransform::Identity;

	/**
	 * Where the left hand holds this weapon. See FRSWeaponPart::LeftHandGrip. Read from the list,
	 * so it is known without anything being built — on the server, which never looks at the weapon.
	 */
	ERSLeftHandGrip GetLeftHandGrip() const;
};
