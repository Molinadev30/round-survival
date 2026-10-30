// Round Survival

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "RSInputConfig.generated.h"

class UInputAction;

/** One Enhanced Input action and the gameplay tag it means. */
USTRUCT(BlueprintType)
struct FRSInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, meta = (Categories = "RS.InputTag"))
	FGameplayTag InputTag;
};

/**
 * Maps input actions to input tags.
 *
 * The split is by who handles the action, not by what it does. Native actions are bound to a
 * C++ function on the character because they are not abilities and never will be — moving and
 * looking. Ability actions are bound generically to a press and a release that forward the tag
 * to the ability system, which is why granting an ability with an input tag is enough to make a
 * key work, with no C++ change anywhere.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	URSInputConfig(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** Bound to a C++ function on the character. Movement and look. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (TitleProperty = "InputAction"))
	TArray<FRSInputAction> NativeInputActions;

	/** Bound generically to the ability system by tag. Fire, reload, aim, interact. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (TitleProperty = "InputAction"))
	TArray<FRSInputAction> AbilityInputActions;
};
