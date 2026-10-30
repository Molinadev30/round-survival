// Round Survival

#include "Interaction/RSBarricadeWindowLayout.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "Interaction/RSBarricade.h"
#include "UObject/UObjectIterator.h"
#endif

#if WITH_EDITOR
void URSBarricadeWindowLayout::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// A barricade only reads its layout when it is constructed, so without this an edit to a board
	// or a reach gap shows nowhere until something else happens to rebuild the actor. Editor worlds
	// only: a running PIE copy keeps the window it started with.
	for (TObjectIterator<ARSBarricade> It; It; ++It)
	{
		ARSBarricade* Barricade = *It;
		const UWorld* World = Barricade ? Barricade->GetWorld() : nullptr;
		if (IsValid(Barricade) && !Barricade->IsTemplate() && Barricade->GetWindowLayout() == this
			&& World && World->WorldType == EWorldType::Editor)
		{
			Barricade->RerunConstructionScripts();
		}
	}
}
#endif
