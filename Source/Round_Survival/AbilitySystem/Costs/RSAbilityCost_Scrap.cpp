// Round Survival

#include "AbilitySystem/Costs/RSAbilityCost_Scrap.h"

#include "AbilitySystem/Abilities/RSGameplayAbility_Interact.h"
#include "AbilitySystem/Attributes/RSEconomySet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Interaction/RSInteractable.h"
#include "Interaction/RSInteractorComponent.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

namespace
{
	/**
	 * What the ability is about to interact with.
	 *
	 * The ability's own target is asked first, because by the time the hold completes that is the
	 * one thing known to have been validated. Before activation there is no target yet, so the
	 * focus the interactor component is already keeping stands in — the same answer the ability
	 * would reach a moment later.
	 */
	AActor* FindInteractionTarget(const URSGameplayAbility* Ability, const FGameplayAbilityActorInfo* ActorInfo)
	{
		if (const URSGameplayAbility_Interact* InteractAbility = Cast<URSGameplayAbility_Interact>(Ability))
		{
			if (AActor* Target = InteractAbility->GetCurrentInteractionTarget())
			{
				return Target;
			}
		}

		AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		const URSInteractorComponent* Interactor = Avatar ? Avatar->FindComponentByClass<URSInteractorComponent>() : nullptr;

		return Interactor ? Interactor->GetFocusedInteractable() : nullptr;
	}
}

URSAbilityCost_Scrap::URSAbilityCost_Scrap()
{
	FailureTag = RSGameplayTags::Ability_ActivateFail_NoScrap;
}

int32 URSAbilityCost_Scrap::GetPendingCost(const URSGameplayAbility* Ability, const FGameplayAbilityActorInfo* ActorInfo)
{
	AActor* Target = FindInteractionTarget(Ability, ActorInfo);
	if (Target == nullptr || !Target->Implements<URSInteractable>())
	{
		return 0;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	// Negative prices are not a mechanic. A target that answers one is misconfigured, and paying
	// the player for it would be worse than ignoring it.
	return FMath::Max(0, IRSInteractable::Execute_GetScrapCost(Target, Avatar));
}

bool URSAbilityCost_Scrap::CheckCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const int32 Cost = GetPendingCost(Ability, ActorInfo);
	if (Cost <= 0)
	{
		// Free, or nothing in front of the player. Whether there is anything to interact with at
		// all is the ability's question, not the cost's.
		return true;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC == nullptr)
	{
		return false;
	}

	bool bFound = false;
	const float Scrap = ASC->GetGameplayAttributeValue(URSEconomySet::GetScrapAttribute(), bFound);

	const bool bCanPay = bFound && Scrap >= static_cast<float>(Cost);

	if (!bCanPay && OptionalRelevantTags && FailureTag.IsValid())
	{
		OptionalRelevantTags->AddTag(FailureTag);
	}

	return bCanPay;
}

void URSAbilityCost_Scrap::ApplyCost(const URSGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Only the server spends. The client's balance comes back down by replication, and a client
	// that charged itself would show the purchase twice.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	const int32 Cost = GetPendingCost(Ability, ActorInfo);
	if (Cost <= 0)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC == nullptr || !CostEffect)
	{
		UE_LOG(LogRSAbility, Error, TEXT("Scrap cost of %d could not be charged: no ability system or no cost effect set on [%s]."),
			Cost, *GetNameSafe(Ability));
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.SetAbility(Ability);

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(CostEffect, 1.0f, Context);
	if (Spec.IsValid())
	{
		// Negative, because the effect is additive on Scrap: one effect asset pays out and charges
		// depending only on the sign of the magnitude it is handed.
		Spec.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Scrap, -static_cast<float>(Cost));

		// Like the reward tags on an award: tells the HUD's popup what moved the balance.
		Spec.Data->AddDynamicAssetTag(RSGameplayTags::Economy_Spend);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

		UE_LOG(LogRSAbility, Verbose, TEXT("Charged %d scrap for [%s]."), Cost, *GetNameSafe(Ability));
	}
}
