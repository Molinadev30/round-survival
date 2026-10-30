// Round Survival

#pragma once

#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "RSEquipmentInstance.generated.h"

class AActor;
class APawn;
class URSEquipmentDefinition;
struct FRSEquipmentActorToSpawn;

/**
 * A cosmetic actor this equipment spawned, and which side of the camera it belongs to.
 *
 * The flag is kept rather than recomputed: a weapon needs to tell its viewmodel from its world
 * copy to play the right animation on each, and rereading the definition to work that out would
 * assume the two arrays stay in the same order forever.
 */
USTRUCT()
struct FRSSpawnedEquipmentActor
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY()
	bool bFirstPerson = false;
};

/**
 * A live piece of equipment on a pawn.
 *
 * A replicated UObject rather than an actor: equipment state is data, and data does not need a
 * transform, a tick or an actor channel of its own. The visible weapon is a separate cosmetic actor
 * this object owns.
 *
 * It lives only while the equipment is in the pawn's hands. A weapon is created as one of these
 * when it is drawn and destroyed when it is holstered, so nothing that has to outlast a holster —
 * the ammo above all — lives here; that is the weapon item's job, and the item is this instance's
 * instigator.
 *
 * Outered to the pawn, which is how GetPawn and GetWorld resolve.
 */
UCLASS(BlueprintType, Blueprintable)
class ROUND_SURVIVAL_API URSEquipmentInstance : public UObject
{
	GENERATED_BODY()

public:
	URSEquipmentInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UObject interface
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual UWorld* GetWorld() const override final;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of UObject interface

	/** What this was drawn from. For a weapon, the weapon item instance in the player's slot. */
	UFUNCTION(BlueprintPure, Category = "RS|Equipment")
	UObject* GetInstigator() const { return Instigator; }

	/** Server only, before OnEquipped, by the equipment list. */
	void SetInstigator(UObject* InInstigator) { Instigator = InInstigator; }

	UFUNCTION(BlueprintPure, Category = "RS|Equipment")
	TSubclassOf<URSEquipmentDefinition> GetEquipmentDefinition() const { return EquipmentDefinition; }

	/**
	 * Set by the equipment list on every machine before OnEquipped runs.
	 *
	 * Deliberately not replicated on this object: the entry in the replicated equipment list
	 * already carries the class, and reading it from there removes any question of whether this
	 * subobject's own properties have arrived by the time the entry is processed.
	 */
	void SetEquipmentDefinition(TSubclassOf<URSEquipmentDefinition> InDefinition) { EquipmentDefinition = InDefinition; }

	UFUNCTION(BlueprintPure, Category = "RS|Equipment")
	APawn* GetPawn() const;

	UFUNCTION(BlueprintPure, Category = "RS|Equipment", meta = (DeterminesOutputType = PawnType))
	APawn* GetTypedPawn(TSubclassOf<APawn> PawnType) const;

	UFUNCTION(BlueprintPure, Category = "RS|Equipment")
	TArray<AActor*> GetSpawnedActors() const;

	/** The viewmodel copy or the world copy, whichever was asked for. Null if it was not spawned. */
	AActor* GetSpawnedActor(bool bFirstPerson) const;

	/**
	 * Spawns the cosmetic actors on this machine.
	 *
	 * Called from OnEquipped, which runs on the server and on every client that receives the
	 * equipment entry — so each machine builds its own visuals and nothing here replicates.
	 * First-person actors are spawned everywhere but a dedicated server: a dead teammate watching
	 * through this pawn's eyes needs them as much as its own player does.
	 */
	virtual void SpawnEquipmentActors(const TArray<FRSEquipmentActorToSpawn>& ActorsToSpawn);
	virtual void DestroyEquipmentActors();

	virtual void OnEquipped();
	virtual void OnUnequipped();

protected:
	/**
	 * The instigator is known on this machine.
	 *
	 * On the server that is before OnEquipped. On a client the instigator is a replicated pointer to
	 * another subobject and may resolve before or after the equipment entry arrives, so anything that
	 * needs it — a weapon listening to its item's ammo — has to be ready to start from here as well
	 * as from OnEquipped.
	 */
	virtual void OnInstigatorChanged() {}

	UFUNCTION(BlueprintImplementableEvent, Category = "RS|Equipment", meta = (DisplayName = "OnEquipped"))
	void K2_OnEquipped();

	UFUNCTION(BlueprintImplementableEvent, Category = "RS|Equipment", meta = (DisplayName = "OnUnequipped"))
	void K2_OnUnequipped();

private:
	UFUNCTION()
	void OnRep_Instigator();

	UPROPERTY(ReplicatedUsing = OnRep_Instigator)
	TObjectPtr<UObject> Instigator;

	UPROPERTY()
	TSubclassOf<URSEquipmentDefinition> EquipmentDefinition;

	/** Local cosmetics, deliberately not replicated — every machine spawns its own. */
	UPROPERTY()
	TArray<FRSSpawnedEquipmentActor> SpawnedActors;
};
