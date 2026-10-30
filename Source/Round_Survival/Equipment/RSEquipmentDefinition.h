// Round Survival

#pragma once

#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "RSEquipmentDefinition.generated.h"

class AActor;
class URSAbilitySet;
class URSEquipmentInstance;

/**
 * One cosmetic actor spawned when the equipment is equipped.
 *
 * A rifle needs two of these: the viewmodel weapon on the first-person arms, which only the owning
 * player sees, and the world weapon on the body mesh, which everyone except the owner sees.
 */
USTRUCT(BlueprintType)
struct FRSEquipmentActorToSpawn
{
	GENERATED_BODY()

	FRSEquipmentActorToSpawn()
	{
	}

	UPROPERTY(EditAnywhere, Category = "Equipment")
	TSubclassOf<AActor> ActorToSpawn;

	UPROPERTY(EditAnywhere, Category = "Equipment")
	FName AttachSocket;

	UPROPERTY(EditAnywhere, Category = "Equipment")
	FTransform AttachTransform;

	/**
	 * Attaches to the viewmodel arms instead of the body mesh.
	 *
	 * First-person actors are drawn only for a view through the pawn's eyes — its own player's, or
	 * a dead teammate watching in first person — and keep only their montages ticking otherwise.
	 */
	UPROPERTY(EditAnywhere, Category = "Equipment")
	bool bFirstPerson = false;
};

/**
 * What a piece of equipment is: the instance class to create, what it grants, what it spawns.
 *
 * A const CDO-only class, never instantiated — the data lives on the class defaults and the
 * equipment manager reads it through GetDefault. Subclassed in Blueprint per weapon.
 */
UCLASS(Blueprintable, Const, Abstract, BlueprintType)
class ROUND_SURVIVAL_API URSEquipmentDefinition : public UObject
{
	GENERATED_BODY()

public:
	URSEquipmentDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Defaults to URSEquipmentInstance when left empty. Weapons set URSRangedWeaponInstance. */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TSubclassOf<URSEquipmentInstance> InstanceType;

	/**
	 * Granted on equip and revoked on unequip, with the instance as the source object.
	 *
	 * This is how a weapon carries its own fire and reload abilities: the abilities are not on the
	 * pawn, they arrive with the weapon and leave with it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<TObjectPtr<const URSAbilitySet>> AbilitySetsToGrant;

	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<FRSEquipmentActorToSpawn> ActorsToSpawn;
};
