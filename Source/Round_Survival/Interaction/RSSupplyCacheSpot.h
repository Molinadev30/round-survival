// Round Survival

#pragma once

#include "GameFramework/Actor.h"

#include "RSSupplyCacheSpot.generated.h"

class UArrowComponent;
class UBillboardComponent;

/**
 * A place in the level the Supply Cache can move to.
 *
 * A point and a facing, nothing more: the cache stands on it facing along its arrow. The cache lists
 * the spots it may use, so the places it can turn up are the level designer's choice and never
 * picked anywhere else.
 *
 * Invisible in game, and never replicated: it is part of the level, so every machine already has it
 * and the cache can name it across the network.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSSupplyCacheSpot : public AActor
{
	GENERATED_BODY()

public:
	ARSSupplyCacheSpot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
#if WITH_EDITORONLY_DATA
	/** Which way the cache faces. Editor only, like the rest of the spot. */
	UPROPERTY()
	TObjectPtr<UArrowComponent> ArrowComponent;

	UPROPERTY()
	TObjectPtr<UBillboardComponent> BillboardComponent;
#endif
};
