// Round Survival

#include "Animation/RSAnimNotifyState_LeftHandGrip.h"

#include "Animation/RSAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

URSAnimNotifyState_LeftHandGrip::URSAnimNotifyState_LeftHandGrip()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(230, 150, 60);
#endif
}

void URSAnimNotifyState_LeftHandGrip::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (URSAnimInstance* AnimInstance = MeshComp ? Cast<URSAnimInstance>(MeshComp->GetAnimInstance()) : nullptr)
	{
		AnimInstance->ReleaseLeftHandGrip(BlendOutSpeed);
	}
}

void URSAnimNotifyState_LeftHandGrip::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// Also runs when the montage is cut short — a reload cancelled by firing — so the hand always
	// comes back.
	if (URSAnimInstance* AnimInstance = MeshComp ? Cast<URSAnimInstance>(MeshComp->GetAnimInstance()) : nullptr)
	{
		AnimInstance->ReturnLeftHandGrip(BlendReturnSpeed);
	}
}

FString URSAnimNotifyState_LeftHandGrip::GetNotifyName_Implementation() const
{
	return TEXT("Release Left Hand Grip");
}
