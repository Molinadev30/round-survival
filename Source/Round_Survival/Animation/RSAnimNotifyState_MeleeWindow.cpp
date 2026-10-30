// Round Survival

#include "Animation/RSAnimNotifyState_MeleeWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "RSGameplayTags.h"

URSAnimNotifyState_MeleeWindow::URSAnimNotifyState_MeleeWindow()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
#endif
}

void URSAnimNotifyState_MeleeWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		// Melee is resolved on the server. A client playing the same montage would otherwise open
		// a window nobody reads and, once players swing, ask for a hit it has no authority over.
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = RSGameplayTags::Event_Melee_Window;
	EventData.Instigator = Owner;
	EventData.Target = Owner;

	// The whole window in one event: the ability gets told when it opens and for how long, so
	// there is no second notify to lose if the montage is interrupted mid-swing.
	EventData.EventMagnitude = TotalDuration;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, RSGameplayTags::Event_Melee_Window, EventData);
}

#if WITH_EDITOR
FString URSAnimNotifyState_MeleeWindow::GetNotifyName_Implementation() const
{
	return TEXT("Melee Window");
}
#endif
