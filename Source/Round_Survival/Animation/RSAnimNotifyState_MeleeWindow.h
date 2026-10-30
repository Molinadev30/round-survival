// Round Survival

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "RSAnimNotifyState_MeleeWindow.generated.h"

/**
 * Marks the frames of a melee animation that can actually hurt somebody.
 *
 * A swing is not dangerous for its whole length: the wind-up and the recovery are theatre, and
 * only the middle of it connects. Putting that window in the animation rather than in the ability
 * is what makes a swing readable — back off during the wind-up and the blow goes through empty
 * air while the attacker is still committed to the rest of the montage.
 *
 * Sends `RS.Event.Melee.Window` on the owner's ability system with the window's length as the
 * event magnitude, and nothing else. The notify knows when; the ability decides what that means.
 */
UCLASS(meta = (DisplayName = "RS Melee Window"))
class ROUND_SURVIVAL_API URSAnimNotifyState_MeleeWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	URSAnimNotifyState_MeleeWindow();

	//~UAnimNotifyState interface
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
#if WITH_EDITOR
	virtual FString GetNotifyName_Implementation() const override;
#endif
	//~End of UAnimNotifyState interface
};
