// Round Survival

#include "Interaction/RSStationDefinition.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "RSLogChannels.h"

bool URSStationDefinition::IsValidStation() const
{
	return Effect != nullptr && StationTag.IsValid();
}

bool URSStationDefinition::IsGrantedTo(const UAbilitySystemComponent* ASC) const
{
	return ASC && StationTag.IsValid() && ASC->HasMatchingGameplayTag(StationTag);
}

FActiveGameplayEffectHandle URSStationDefinition::GrantTo(UAbilitySystemComponent* ASC) const
{
	if (ASC == nullptr || !ASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogRS, Error, TEXT("[%s] can only be granted on the server, to an ability system."), *GetName());
		return FActiveGameplayEffectHandle();
	}

	if (!IsValidStation())
	{
		UE_LOG(LogRS, Error, TEXT("[%s] needs Effect and StationTag set."), *GetName());
		return FActiveGameplayEffectHandle();
	}

	if (IsGrantedTo(ASC))
	{
		return FActiveGameplayEffectHandle();
	}

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Effect, 1.0f, ASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	// The one place the tag is attached. It replicates with the spec and leaves with the effect.
	SpecHandle.Data->DynamicGrantedTags.AddTag(StationTag);

	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}
