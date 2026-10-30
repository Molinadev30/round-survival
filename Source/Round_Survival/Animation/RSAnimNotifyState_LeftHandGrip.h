// Round Survival

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "RSAnimNotifyState_LeftHandGrip.generated.h"

/**
 * Takes the left hand off the weapon's grip for the length of the window, then puts it back.
 *
 * Placed on the character montages that move the left arm — reload, equip — over the stretch where
 * the hand is actually away from the grip, and no longer. The window closes while the montage is
 * still playing, so the hand returns to the grip under the montage's own motion and never shows the
 * handguard hold the montage settles into at its end. Releasing the grip for as long as a reload or
 * a switch lasted did exactly that: the montage blended out to the handguard first, and only then
 * did the grip fade back in.
 *
 * Its timings and speeds are set on the montages. Does nothing unless the mesh runs a URSAnimInstance,
 * which also keeps it silent in the montage editor's preview.
 */
UCLASS(meta = (DisplayName = "RS Left Hand Grip Release"))
class ROUND_SURVIVAL_API URSAnimNotifyState_LeftHandGrip : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	URSAnimNotifyState_LeftHandGrip();

	//~UAnimNotifyState interface
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	//~End of UAnimNotifyState interface

protected:
	/** How fast the grip fades out as the window opens, as an interpolation speed. */
	UPROPERTY(EditAnywhere, Category = "Left Hand Grip", meta = (ClampMin = 0.0))
	float BlendOutSpeed = 15.0f;

	/** How fast it fades back in as the window closes. */
	UPROPERTY(EditAnywhere, Category = "Left Hand Grip", meta = (ClampMin = 0.0))
	float BlendReturnSpeed = 15.0f;
};
