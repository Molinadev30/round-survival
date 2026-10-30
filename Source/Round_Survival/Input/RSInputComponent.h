// Round Survival

#pragma once

#include "EnhancedInputComponent.h"
#include "Input/RSInputConfig.h"

#include "RSInputComponent.generated.h"

/**
 * Input component that binds by gameplay tag instead of by name.
 *
 * BindAbilityActions walks the config and binds every ability action to the same pair of
 * handlers, passing the tag along. That is the whole reason adding an ability with an input
 * touches no C++: the binding code never names Fire, Reload or Interact.
 *
 * Bind handles come back to the caller so a temporary input set — a downed state, a mounted
 * turret — can be removed without tearing down every other binding.
 */
UCLASS(Config = Input)
class ROUND_SURVIVAL_API URSInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	URSInputComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	template<class UserClass, typename FuncType>
	void BindNativeAction(const URSInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent,
		UserClass* Object, FuncType Func, bool bLogIfNotFound = true);

	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const URSInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc,
		ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles);

	void RemoveBinds(TArray<uint32>& BindHandles);
};

template<class UserClass, typename FuncType>
void URSInputComponent::BindNativeAction(const URSInputConfig* InputConfig, const FGameplayTag& InputTag,
	ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound)
{
	check(InputConfig);

	if (const UInputAction* InputAction = InputConfig->FindNativeInputActionForTag(InputTag, bLogIfNotFound))
	{
		BindAction(InputAction, TriggerEvent, Object, Func);
	}
}

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void URSInputComponent::BindAbilityActions(const URSInputConfig* InputConfig, UserClass* Object,
	PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles)
{
	check(InputConfig);

	for (const FRSInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (!Action.InputAction || !Action.InputTag.IsValid())
		{
			continue;
		}

		if (PressedFunc)
		{
			BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag).GetHandle());
		}

		if (ReleasedFunc)
		{
			BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
		}
	}
}
