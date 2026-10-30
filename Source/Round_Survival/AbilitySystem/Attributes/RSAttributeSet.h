// Round Survival

#pragma once

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "RSAttributeSet.generated.h"

class URSAbilitySystemComponent;

/**
 * Generates the standard accessor set for an attribute.
 *
 *   ATTRIBUTE_ACCESSORS(URSHealthSet, Health)
 *
 * produces GetHealthAttribute(), GetHealth(), SetHealth() and InitHealth().
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Base class for every Round Survival attribute set.
 *
 * Exists so that attribute sets share the owner accessors and so that adding
 * cross-cutting behaviour later touches one place instead of three.
 */
UCLASS()
class ROUND_SURVIVAL_API URSAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	URSAttributeSet();

	UWorld* GetWorld() const override;

	/** The ability system component that owns this set, already cast to ours. */
	URSAbilitySystemComponent* GetRSAbilitySystemComponent() const;
};
