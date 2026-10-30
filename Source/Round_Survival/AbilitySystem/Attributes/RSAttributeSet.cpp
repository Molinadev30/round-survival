// Round Survival

#include "AbilitySystem/Attributes/RSAttributeSet.h"

#include "AbilitySystem/RSAbilitySystemComponent.h"

URSAttributeSet::URSAttributeSet()
{
}

UWorld* URSAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	if (!ensure(Outer))
	{
		return nullptr;
	}

	return Outer->GetWorld();
}

URSAbilitySystemComponent* URSAttributeSet::GetRSAbilitySystemComponent() const
{
	return Cast<URSAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}
