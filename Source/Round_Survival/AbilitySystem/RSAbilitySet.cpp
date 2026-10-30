// Round Survival

#include "AbilitySystem/RSAbilitySet.h"

#include "AbilitySystem/Abilities/RSGameplayAbility.h"
#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "RSLogChannels.h"

void FRSAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FRSAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FRSAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	GrantedAttributeSets.Add(Set);
}

void FRSAbilitySet_GrantedHandles::TakeFromAbilitySystem(URSAbilitySystemComponent* RSASC)
{
	check(RSASC);

	if (!RSASC->IsOwnerActorAuthoritative())
	{
		// Removing on a client would desync the ability list until the next full replication.
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			RSASC->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			RSASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	for (UAttributeSet* Set : GrantedAttributeSets)
	{
		RSASC->RemoveSpawnedAttribute(Set);
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	GrantedAttributeSets.Reset();
}

URSAbilitySet::URSAbilitySet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URSAbilitySet::GiveToAbilitySystem(URSAbilitySystemComponent* RSASC, FRSAbilitySet_GrantedHandles* OutGrantedHandles,
	UObject* SourceObject) const
{
	check(RSASC);

	if (!RSASC->IsOwnerActorAuthoritative())
	{
		// Granting is a server-only operation. Clients receive the results by replication.
		return;
	}

	// Attribute sets first: an effect or ability granted below may immediately reference an
	// attribute that does not exist yet otherwise.
	for (int32 Index = 0; Index < GrantedAttributes.Num(); ++Index)
	{
		const FRSAbilitySet_AttributeSet& SetToGrant = GrantedAttributes[Index];

		if (!IsValid(SetToGrant.AttributeSet))
		{
			UE_LOG(LogRSAbility, Error, TEXT("GrantedAttributes[%d] on ability set [%s] is not valid."), Index, *GetNameSafe(this));
			continue;
		}

		UAttributeSet* NewSet = NewObject<UAttributeSet>(RSASC->GetOwner(), SetToGrant.AttributeSet);
		RSASC->AddAttributeSetSubobject(NewSet);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAttributeSet(NewSet);
		}
	}

	for (int32 Index = 0; Index < GrantedGameplayAbilities.Num(); ++Index)
	{
		const FRSAbilitySet_GameplayAbility& AbilityToGrant = GrantedGameplayAbilities[Index];

		if (!IsValid(AbilityToGrant.Ability))
		{
			UE_LOG(LogRSAbility, Error, TEXT("GrantedGameplayAbilities[%d] on ability set [%s] is not valid."), Index, *GetNameSafe(this));
			continue;
		}

		URSGameplayAbility* AbilityCDO = AbilityToGrant.Ability->GetDefaultObject<URSGameplayAbility>();

		FGameplayAbilitySpec AbilitySpec(AbilityCDO, AbilityToGrant.AbilityLevel);
		AbilitySpec.SourceObject = SourceObject;

		// The input tag rides on the spec as a dynamic tag, which is how the input component
		// finds the right ability to activate without anything holding a hardcoded list.
		if (AbilityToGrant.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);
		}

		const FGameplayAbilitySpecHandle AbilitySpecHandle = RSASC->GiveAbility(AbilitySpec);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilitySpecHandle);
		}
	}

	for (int32 Index = 0; Index < GrantedGameplayEffects.Num(); ++Index)
	{
		const FRSAbilitySet_GameplayEffect& EffectToGrant = GrantedGameplayEffects[Index];

		if (!IsValid(EffectToGrant.GameplayEffect))
		{
			UE_LOG(LogRSAbility, Error, TEXT("GrantedGameplayEffects[%d] on ability set [%s] is not valid."), Index, *GetNameSafe(this));
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle EffectHandle =
			RSASC->ApplyGameplayEffectToSelf(GameplayEffect, EffectToGrant.EffectLevel, RSASC->MakeEffectContext());

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(EffectHandle);
		}
	}
}
