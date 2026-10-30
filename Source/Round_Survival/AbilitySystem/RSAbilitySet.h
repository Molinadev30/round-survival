// Round Survival

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"

#include "RSAbilitySet.generated.h"

class UAttributeSet;
class UGameplayEffect;
class URSAbilitySystemComponent;
class URSGameplayAbility;

/** One ability to grant, with the input tag that will activate it. */
USTRUCT(BlueprintType)
struct FRSAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<URSGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	/** Input tag that activates it. Leave empty for abilities with no input, such as passives. */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "RS.InputTag"))
	FGameplayTag InputTag;
};

/** One gameplay effect to apply for as long as the set is granted. */
USTRUCT(BlueprintType)
struct FRSAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};

/** One attribute set to add to the ability system component. */
USTRUCT(BlueprintType)
struct FRSAbilitySet_AttributeSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAttributeSet> AttributeSet;
};

/**
 * Everything a single grant handed out, so it can all be taken back cleanly.
 *
 * Without this, unequipping a weapon would mean remembering which abilities and effects it
 * happened to add — the usual source of abilities that outlive the thing that granted them.
 */
USTRUCT(BlueprintType)
struct FRSAbilitySet_GrantedHandles
{
	GENERATED_BODY()

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	/** Removes everything this grant added. Server only. */
	void TakeFromAbilitySystem(URSAbilitySystemComponent* RSASC);

protected:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

/**
 * A block of abilities, effects and attribute sets granted together.
 *
 * Used for the player's starting kit, for what a weapon adds when it is equipped, and for the
 * enemy kit applied at spawn. Adding a new ability to a weapon is then editing a data asset.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URSAbilitySet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Grants the whole set. Server only.
	 *
	 * @param SourceObject  What is granting this, e.g. the weapon instance. Abilities read it
	 *                      back to find the equipment they belong to.
	 */
	void GiveToAbilitySystem(URSAbilitySystemComponent* RSASC, FRSAbilitySet_GrantedHandles* OutGrantedHandles,
		UObject* SourceObject = nullptr) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta = (TitleProperty = Ability))
	TArray<FRSAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects", meta = (TitleProperty = GameplayEffect))
	TArray<FRSAbilitySet_GameplayEffect> GrantedGameplayEffects;

	UPROPERTY(EditDefaultsOnly, Category = "Attribute Sets", meta = (TitleProperty = AttributeSet))
	TArray<FRSAbilitySet_AttributeSet> GrantedAttributes;
};
