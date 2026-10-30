// Round Survival

#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"

#include "RSWeaponTargetData.generated.h"

/**
 * One confirmed bullet impact on its way from the shooter to the server.
 *
 * The engine's single-target hit carries the FHitResult; this adds the cartridge it belonged to,
 * so a shotgun's pellets can be told apart from separate trigger pulls when the server validates
 * them and when the cues play.
 */
USTRUCT()
struct FRSGameplayAbilityTargetData_SingleTargetHit : public FGameplayAbilityTargetData_SingleTargetHit
{
	GENERATED_BODY()

	FRSGameplayAbilityTargetData_SingleTargetHit()
		: CartridgeID(-1)
	{
	}

	virtual void AddTargetDataToContext(FGameplayEffectContextHandle& Context, bool bIncludeActorArray) const override;

	/** Which trigger pull this bullet belonged to. */
	UPROPERTY()
	int32 CartridgeID;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FRSGameplayAbilityTargetData_SingleTargetHit::StaticStruct();
	}
};

template<>
struct TStructOpsTypeTraits<FRSGameplayAbilityTargetData_SingleTargetHit> : public TStructOpsTypeTraitsBase2<FRSGameplayAbilityTargetData_SingleTargetHit>
{
	enum
	{
		// Required: without it the handle's net serialization drops this type back to the base struct.
		WithNetSerializer = true
	};
};
