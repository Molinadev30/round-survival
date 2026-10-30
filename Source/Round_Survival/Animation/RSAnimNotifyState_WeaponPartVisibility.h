// Round Survival

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "RSAnimNotifyState_WeaponPartVisibility.generated.h"

/**
 * Shows or hides one weapon part for the length of the notify window, then puts it back.
 *
 * Placed on a weapon montage, not driven by the ability that plays it. The animation is what moves
 * the part's bone, so the animation is also what decides when the part should be seen: the window
 * opens and closes in the same animation update that moves the bone, and a montage that is cut
 * short closes its windows on the way out. Timing this from gameplay code instead ran on a second
 * clock and drifted from the pose by a frame at the end of every reload.
 *
 * Does nothing unless the mesh is a `URSWeaponAssemblyComponent`, which also keeps it silent in
 * the montage editor's preview.
 */
UCLASS(meta = (DisplayName = "RS Weapon Part Visibility"))
class ROUND_SURVIVAL_API URSAnimNotifyState_WeaponPartVisibility : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	URSAnimNotifyState_WeaponPartVisibility();

	//~UAnimNotifyState interface
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	//~End of UAnimNotifyState interface

protected:
	/** A `PartName` from the weapon assembly's `Parts` list. */
	UPROPERTY(EditAnywhere, Category = "Weapon Part")
	FName PartName;

	/** True to show the part inside the window, false to hide it. The opposite applies outside. */
	UPROPERTY(EditAnywhere, Category = "Weapon Part")
	bool bVisibleDuringWindow = true;

private:
	void SetPartVisible(USkeletalMeshComponent* MeshComp, bool bVisible) const;
};
