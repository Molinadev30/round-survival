// Round Survival

#include "AbilitySystem/RSGameplayTagStack.h"

#include "RSLogChannels.h"

FString FRSGameplayTagStack::GetDebugString() const
{
	return FString::Printf(TEXT("%s x%d"), *Tag.ToString(), StackCount);
}

void FRSGameplayTagStackContainer::AddStack(FGameplayTag Tag, int32 StackCount)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogRSAbility, Warning, TEXT("AddStack called with an invalid tag."));
		return;
	}

	if (StackCount <= 0)
	{
		return;
	}

	for (FRSGameplayTagStack& Stack : Stacks)
	{
		if (Stack.Tag == Tag)
		{
			Stack.StackCount += StackCount;
			TagToCountMap[Tag] = Stack.StackCount;
			MarkItemDirty(Stack);
			return;
		}
	}

	FRSGameplayTagStack& NewStack = Stacks.Emplace_GetRef(Tag, StackCount);
	MarkItemDirty(NewStack);
	TagToCountMap.Add(Tag, StackCount);
}

void FRSGameplayTagStackContainer::RemoveStack(FGameplayTag Tag, int32 StackCount)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogRSAbility, Warning, TEXT("RemoveStack called with an invalid tag."));
		return;
	}

	if (StackCount <= 0)
	{
		return;
	}

	for (auto It = Stacks.CreateIterator(); It; ++It)
	{
		FRSGameplayTagStack& Stack = *It;

		if (Stack.Tag == Tag)
		{
			if (Stack.StackCount <= StackCount)
			{
				// Removing the entry rather than leaving a zero: an empty magazine and a weapon
				// that never had one answer GetStackCount identically, so a zero entry is noise.
				It.RemoveCurrent();
				TagToCountMap.Remove(Tag);
				MarkArrayDirty();
			}
			else
			{
				Stack.StackCount -= StackCount;
				TagToCountMap[Tag] = Stack.StackCount;
				MarkItemDirty(Stack);
			}

			return;
		}
	}
}

FString FRSGameplayTagStackContainer::GetDebugString() const
{
	TArray<FString> Entries;
	Entries.Reserve(Stacks.Num());

	for (const FRSGameplayTagStack& Stack : Stacks)
	{
		Entries.Add(Stack.GetDebugString());
	}

	return FString::Join(Entries, TEXT(", "));
}

void FRSGameplayTagStackContainer::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	// Run before the entries leave the array, so the tags are still readable here.
	for (int32 Index : RemovedIndices)
	{
		TagToCountMap.Remove(Stacks[Index].Tag);
	}
}

void FRSGameplayTagStackContainer::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		const FRSGameplayTagStack& Stack = Stacks[Index];
		TagToCountMap.Add(Stack.Tag, Stack.StackCount);
	}
}

void FRSGameplayTagStackContainer::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		const FRSGameplayTagStack& Stack = Stacks[Index];
		TagToCountMap[Stack.Tag] = Stack.StackCount;
	}
}
