// Round Survival

#include "Interaction/RSSupplyCacheSpot.h"

#include "Components/SceneComponent.h"

#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#endif

ARSSupplyCacheSpot::ARSSupplyCacheSpot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	bReplicates = false;
	SetHidden(true);

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (ArrowComponent)
	{
		ArrowComponent->SetupAttachment(RootComponent);
		ArrowComponent->ArrowSize = 1.5f;
		ArrowComponent->SetArrowColor(FLinearColor(0.2f, 0.6f, 1.0f));
		ArrowComponent->bIsScreenSizeScaled = true;
	}

	BillboardComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (BillboardComponent)
	{
		BillboardComponent->SetupAttachment(RootComponent);
		BillboardComponent->bIsScreenSizeScaled = true;
	}
#endif
}
