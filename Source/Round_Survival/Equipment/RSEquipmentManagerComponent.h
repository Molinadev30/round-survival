// Round Survival

#pragma once

#include "AbilitySystem/RSAbilitySet.h"
#include "Components/PawnComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Templates/SubclassOf.h"

#include "RSEquipmentManagerComponent.generated.h"

class UActorComponent;
class URSAbilitySystemComponent;
class URSEquipmentDefinition;
class URSEquipmentInstance;
class URSEquipmentManagerComponent;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;
struct FRSEquipmentList;

/** One equipped item: what it was, the live instance, and what its ability sets granted. */
USTRUCT(BlueprintType)
struct FRSAppliedEquipmentEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FRSAppliedEquipmentEntry()
	{
	}

	FString GetDebugString() const;

private:
	friend FRSEquipmentList;
	friend URSEquipmentManagerComponent;

	UPROPERTY()
	TSubclassOf<URSEquipmentDefinition> EquipmentDefinition;

	UPROPERTY()
	TObjectPtr<URSEquipmentInstance> Instance = nullptr;

	/** Server-side bookkeeping. A client never revokes anything, so it never needs these. */
	UPROPERTY(NotReplicated)
	FRSAbilitySet_GrantedHandles GrantedHandles;
};

/** The replicated list of equipped items. */
USTRUCT(BlueprintType)
struct FRSEquipmentList : public FFastArraySerializer
{
	GENERATED_BODY()

	FRSEquipmentList()
		: OwnerComponent(nullptr)
	{
	}

	FRSEquipmentList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRSAppliedEquipmentEntry, FRSEquipmentList>(Entries, DeltaParms, *this);
	}

	/** Server only: creates the instance and grants the definition's ability sets. */
	URSEquipmentInstance* AddEntry(TSubclassOf<URSEquipmentDefinition> EquipmentDefinition, UObject* Instigator);
	void RemoveEntry(URSEquipmentInstance* Instance);

private:
	URSAbilitySystemComponent* GetAbilitySystemComponent() const;

	friend URSEquipmentManagerComponent;

	UPROPERTY()
	TArray<FRSAppliedEquipmentEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FRSEquipmentList> : public TStructOpsTypeTraitsBase2<FRSEquipmentList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * Holds what a pawn has equipped.
 *
 * Equipping is a server-only operation: it creates the instance, grants the ability sets and
 * marks the entry dirty. Clients learn about it through the fast array and build their own
 * cosmetics from it. A client that calls EquipItem does nothing at all — the ability set that
 * would let it shoot only ever exists on the server's ability system.
 */
UCLASS(BlueprintType)
class ROUND_SURVIVAL_API URSEquipmentManagerComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URSEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Draws a piece of equipment. Server only.
	 *
	 * @param Instigator  What it was drawn from — for a weapon, its item instance. Set on the new
	 *                    instance before it is equipped or any ability is granted, so nothing that
	 *                    runs during the equip finds it missing.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Equipment")
	URSEquipmentInstance* EquipItem(TSubclassOf<URSEquipmentDefinition> EquipmentDefinition, UObject* Instigator = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "RS|Equipment")
	void UnequipItem(URSEquipmentInstance* ItemInstance);

	/** First equipped instance of a type, or null. The fire ability finds its weapon this way. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RS|Equipment")
	URSEquipmentInstance* GetFirstInstanceOfType(TSubclassOf<URSEquipmentInstance> InstanceType) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RS|Equipment")
	TArray<URSEquipmentInstance*> GetEquipmentInstancesOfType(TSubclassOf<URSEquipmentInstance> InstanceType) const;

	template <typename T>
	T* GetFirstInstanceOfType() const
	{
		return Cast<T>(GetFirstInstanceOfType(T::StaticClass()));
	}

	//~UObject interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	//~End of UObject interface

	//~UActorComponent interface
	virtual void UninitializeComponent() override;
	virtual void ReadyForReplication() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UActorComponent interface

	/**
	 * Turns ticking on only while a ranged weapon is equipped.
	 *
	 * Spread is the one thing in this project that genuinely needs a per-frame update: heat decay
	 * and the aiming / crouching / movement multipliers are interpolations, and sampling them once
	 * per shot would make the reticle jump. Everything else still runs on events and timers.
	 */
	void UpdateTickState();

private:
	UPROPERTY(Replicated)
	FRSEquipmentList EquipmentList;
};
