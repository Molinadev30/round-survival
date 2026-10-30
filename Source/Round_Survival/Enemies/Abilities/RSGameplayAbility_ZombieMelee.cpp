// Round Survival

#include "Enemies/Abilities/RSGameplayAbility_ZombieMelee.h"

#include "AbilitySystemComponent.h"
#include "Enemies/RSZombieCharacter.h"
#include "Interaction/RSBarricade.h"
#include "RSGameplayTags.h"

void URSGameplayAbility_ZombieMelee::GetLineOfSightExceptions(TArray<AActor*>& OutActors) const
{
	Super::GetLineOfSightExceptions(OutActors);

	const ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(GetAvatarActorFromActorInfo());
	ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	if (IsValid(Barricade))
	{
		OutActors.Add(Barricade);
	}
}

UAnimMontage* URSGameplayAbility_ZombieMelee::GetAttackMontage() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (CrawlingAttackMontage && ASC && ASC->HasMatchingGameplayTag(RSGameplayTags::State_Crawling))
	{
		return CrawlingAttackMontage;
	}

	return Super::GetAttackMontage();
}
