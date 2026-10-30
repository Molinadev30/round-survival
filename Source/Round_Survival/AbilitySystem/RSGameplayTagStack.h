// Round Survival

#pragma once

#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "RSGameplayTagStack.generated.h"

struct FRSGameplayTagStackContainer;
struct FNetDeltaSerializeInfo;

/**
 * One tag with a count attached: RS.Weapon.Ammo.Magazine x30.
 *
 * A gameplay tag container answers "is this tag present". This answers "how many", which is what
 * a magazine needs and what an attribute cannot provide without giving the pawn an ammo attribute
 * per carried weapon.
 */
USTRUCT(BlueprintType)
struct FRSGameplayTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FRSGameplayTagStack()
	{
	}

	FRSGameplayTagStack(FGameplayTag InTag, int32 InStackCount)
		: Tag(InTag)
		, StackCount(InStackCount)
	{
	}

	FString GetDebugString() const;

private:
	friend FRSGameplayTagStackContainer;

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 StackCount = 0;
};

/**
 * A replicated set of tag stacks, living on an equipment instance.
 *
 * This is a fast array rather than a replicated TMap because a TMap has no delta serialiser: every
 * shot fired would resend the whole map. The fast array sends only the entry that changed, which
 * matters for a stack that changes with every shot.
 *
 * TagToCountMap is a local acceleration structure, rebuilt from the replication callbacks. It is
 * deliberately not replicated — it is derived state, and shipping both would be two sources of
 * truth for the same number.
 */
USTRUCT(BlueprintType)
struct FRSGameplayTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	FRSGameplayTagStackContainer()
	{
	}

public:
	/** Adds StackCount to Tag, creating the entry if it is missing. Does nothing below 1. */
	void AddStack(FGameplayTag Tag, int32 StackCount);

	/** Removes StackCount from Tag, dropping the entry at zero. Does nothing below 1. */
	void RemoveStack(FGameplayTag Tag, int32 StackCount);

	/** Count for Tag, or 0 when absent. Zero and absent are the same answer on purpose. */
	int32 GetStackCount(FGameplayTag Tag) const
	{
		return TagToCountMap.FindRef(Tag);
	}

	bool ContainsTag(FGameplayTag Tag) const
	{
		return TagToCountMap.Contains(Tag);
	}

	/** Every stack as "Tag xN", for showdebug and logging. */
	FString GetDebugString() const;

	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRSGameplayTagStack, FRSGameplayTagStackContainer>(Stacks, DeltaParms, *this);
	}

private:
	UPROPERTY()
	TArray<FRSGameplayTagStack> Stacks;

	/** Queries are O(1) through this; the array is the replicated truth. */
	TMap<FGameplayTag, int32> TagToCountMap;
};

template<>
struct TStructOpsTypeTraits<FRSGameplayTagStackContainer> : public TStructOpsTypeTraitsBase2<FRSGameplayTagStackContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
