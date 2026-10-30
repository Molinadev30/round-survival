// Round Survival

#pragma once

#include "Components/StateTreeAIComponentSchema.h"

#include "RSStateTreeSchema_Zombie.generated.h"

/**
 * The schema every enemy behaviour tree is built against.
 *
 * A schema is what a State Tree asset is allowed to talk about. The engine's AI schema hands a
 * tree an APawn and an AAIController; this one narrows both to our classes, so a task binds
 * straight to the enemy's own properties and its targeting component without a cast, and the
 * editor refuses to let a tree built for something else run on an enemy.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Round Survival Enemy"))
class ROUND_SURVIVAL_API URSStateTreeSchema_Zombie : public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	URSStateTreeSchema_Zombie(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
