// Round Survival

#include "Animation/RSAnimNotifyState_WeaponPartVisibility.h"

#include "Weapons/RSWeaponAssemblyComponent.h"

URSAnimNotifyState_WeaponPartVisibility::URSAnimNotifyState_WeaponPartVisibility()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(90, 170, 230);
#endif
}

void URSAnimNotifyState_WeaponPartVisibility::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	SetPartVisible(MeshComp, bVisibleDuringWindow);
}

void URSAnimNotifyState_WeaponPartVisibility::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	SetPartVisible(MeshComp, !bVisibleDuringWindow);
}

FString URSAnimNotifyState_WeaponPartVisibility::GetNotifyName_Implementation() const
{
	// Shown on the montage timeline, where several of these sit side by side.
	return FString::Printf(TEXT("%s %s"), bVisibleDuringWindow ? TEXT("Show") : TEXT("Hide"), *PartName.ToString());
}

void URSAnimNotifyState_WeaponPartVisibility::SetPartVisible(USkeletalMeshComponent* MeshComp, bool bVisible) const
{
	// The montage plays on the receiver, which is the assembly. The viewmodel and the world copy
	// each run their own montage, so each one fixes only itself.
	if (URSWeaponAssemblyComponent* Assembly = Cast<URSWeaponAssemblyComponent>(MeshComp))
	{
		Assembly->SetPartHidden(PartName, !bVisible);
	}
}
