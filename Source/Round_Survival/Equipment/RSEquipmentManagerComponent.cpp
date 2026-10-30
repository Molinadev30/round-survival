// Round Survival

#include "Equipment/RSEquipmentManagerComponent.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "Engine/ActorChannel.h"
#include "Equipment/RSEquipmentDefinition.h"
#include "Equipment/RSEquipmentInstance.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/RSRangedWeaponInstance.h"
#include "RSLogChannels.h"

FString FRSAppliedEquipmentEntry::GetDebugString() const
{
	return FString::Printf(TEXT("%s of %s"), *GetNameSafe(Instance), *GetNameSafe(EquipmentDefinition.Get()));
}

void FRSEquipmentList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		if (URSEquipmentInstance* Instance = Entries[Index].Instance)
		{
			Instance->OnUnequipped();
		}
	}
}

void FRSEquipmentList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		const FRSAppliedEquipmentEntry& Entry = Entries[Index];

		if (URSEquipmentInstance* Instance = Entry.Instance)
		{
			// The definition comes off the entry rather than the instance, so the client never
			// depends on the order the subobject's own properties happened to arrive in.
			Instance->SetEquipmentDefinition(Entry.EquipmentDefinition);
			Instance->OnEquipped();

			if (URSEquipmentManagerComponent* Manager = Cast<URSEquipmentManagerComponent>(OwnerComponent))
			{
				Manager->UpdateTickState();
			}
		}
	}
}

URSAbilitySystemComponent* FRSEquipmentList::GetAbilitySystemComponent() const
{
	check(OwnerComponent);

	AActor* OwningActor = OwnerComponent->GetOwner();
	return Cast<URSAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor));
}

URSEquipmentInstance* FRSEquipmentList::AddEntry(TSubclassOf<URSEquipmentDefinition> EquipmentDefinition, UObject* Instigator)
{
	check(EquipmentDefinition);
	check(OwnerComponent);
	check(OwnerComponent->GetOwner()->HasAuthority());

	const URSEquipmentDefinition* EquipmentCDO = GetDefault<URSEquipmentDefinition>(EquipmentDefinition);

	TSubclassOf<URSEquipmentInstance> InstanceType = EquipmentCDO->InstanceType;
	if (InstanceType == nullptr)
	{
		InstanceType = URSEquipmentInstance::StaticClass();
	}

	FRSAppliedEquipmentEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.EquipmentDefinition = EquipmentDefinition;

	// Outered to the pawn, not to this component: a UObject subobject of a component does not
	// replicate reliably, and the instance resolves its pawn through GetOuter.
	NewEntry.Instance = NewObject<URSEquipmentInstance>(OwnerComponent->GetOwner(), InstanceType);
	NewEntry.Instance->SetEquipmentDefinition(EquipmentDefinition);

	// Before the abilities exist: a fire ability that checked its cost on the frame it was granted
	// reaches its ammo through this, and would otherwise report the weapon as empty.
	NewEntry.Instance->SetInstigator(Instigator);

	if (URSAbilitySystemComponent* RSASC = GetAbilitySystemComponent())
	{
		for (const TObjectPtr<const URSAbilitySet>& AbilitySet : EquipmentCDO->AbilitySetsToGrant)
		{
			if (AbilitySet)
			{
				// The instance is the source object, which is how the fire ability finds the
				// weapon it was granted by without anyone holding a pointer to it.
				AbilitySet->GiveToAbilitySystem(RSASC, &NewEntry.GrantedHandles, NewEntry.Instance);
			}
		}
	}
	else
	{
		UE_LOG(LogRSAbility, Warning, TEXT("Equipping [%s] on [%s]: no ability system component, nothing granted."),
			*GetNameSafe(EquipmentDefinition), *GetNameSafe(OwnerComponent->GetOwner()));
	}

	MarkItemDirty(NewEntry);

	return NewEntry.Instance;
}

void FRSEquipmentList::RemoveEntry(URSEquipmentInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRSAppliedEquipmentEntry& Entry = *EntryIt;

		if (Entry.Instance == Instance)
		{
			if (URSAbilitySystemComponent* RSASC = GetAbilitySystemComponent())
			{
				Entry.GrantedHandles.TakeFromAbilitySystem(RSASC);
			}

			EntryIt.RemoveCurrent();
			MarkArrayDirty();
			return;
		}
	}
}

URSEquipmentManagerComponent::URSEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EquipmentList(this)
{
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;

	// Enabled only while a ranged weapon is equipped; see UpdateTickState.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URSEquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, EquipmentList);
}

URSEquipmentInstance* URSEquipmentManagerComponent::EquipItem(TSubclassOf<URSEquipmentDefinition> EquipmentDefinition, UObject* Instigator)
{
	if (!EquipmentDefinition)
	{
		return nullptr;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogRSAbility, Warning, TEXT("EquipItem called without authority on [%s]; ignored."), *GetNameSafe(GetOwner()));
		return nullptr;
	}

	URSEquipmentInstance* Result = EquipmentList.AddEntry(EquipmentDefinition, Instigator);
	if (Result)
	{
		// The server runs this path itself; clients reach it from PostReplicatedAdd.
		Result->OnEquipped();

		if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
		{
			AddReplicatedSubObject(Result);
		}

		UpdateTickState();
	}

	return Result;
}

void URSEquipmentManagerComponent::UnequipItem(URSEquipmentInstance* ItemInstance)
{
	if (!ItemInstance)
	{
		return;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogRSAbility, Warning, TEXT("UnequipItem called without authority on [%s]; ignored."), *GetNameSafe(GetOwner()));
		return;
	}

	if (IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}

	ItemInstance->OnUnequipped();
	EquipmentList.RemoveEntry(ItemInstance);
}

URSEquipmentInstance* URSEquipmentManagerComponent::GetFirstInstanceOfType(TSubclassOf<URSEquipmentInstance> InstanceType) const
{
	if (!InstanceType)
	{
		return nullptr;
	}

	for (const FRSAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (URSEquipmentInstance* Instance = Entry.Instance)
		{
			if (Instance->IsA(InstanceType))
			{
				return Instance;
			}
		}
	}

	return nullptr;
}

TArray<URSEquipmentInstance*> URSEquipmentManagerComponent::GetEquipmentInstancesOfType(TSubclassOf<URSEquipmentInstance> InstanceType) const
{
	TArray<URSEquipmentInstance*> Results;

	if (!InstanceType)
	{
		return Results;
	}

	for (const FRSAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (URSEquipmentInstance* Instance = Entry.Instance)
		{
			if (Instance->IsA(InstanceType))
			{
				Results.Add(Instance);
			}
		}
	}

	return Results;
}

bool URSEquipmentManagerComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (FRSAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (IsValid(Entry.Instance))
		{
			bWroteSomething |= Channel->ReplicateSubobject(Entry.Instance, *Bunch, *RepFlags);
		}
	}

	return bWroteSomething;
}

void URSEquipmentManagerComponent::UninitializeComponent()
{
	// Copied out first: unequipping mutates the list this would otherwise be iterating.
	TArray<URSEquipmentInstance*> AllInstances;
	AllInstances.Reserve(EquipmentList.Entries.Num());

	for (const FRSAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		AllInstances.Add(Entry.Instance);
	}

	const bool bHasAuthority = GetOwner() && GetOwner()->HasAuthority();

	for (URSEquipmentInstance* Instance : AllInstances)
	{
		if (bHasAuthority)
		{
			UnequipItem(Instance);
		}
		else if (Instance)
		{
			// A client cannot revoke abilities or touch the replicated list, but it did spawn the
			// cosmetic actors itself and is the only one that can tear them down.
			Instance->OnUnequipped();
		}
	}

	Super::UninitializeComponent();
}

void URSEquipmentManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	int32 TickedWeapons = 0;

	for (const FRSAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (URSRangedWeaponInstance* RangedWeapon = Cast<URSRangedWeaponInstance>(Entry.Instance))
		{
			RangedWeapon->Tick(DeltaTime);
			++TickedWeapons;
		}
	}

	// Switching off here rather than from the removal path covers both sides: the server unequips
	// through UnequipItem, a client only ever finds out from the replicated list.
	if (TickedWeapons == 0)
	{
		SetComponentTickEnabled(false);
	}
}

void URSEquipmentManagerComponent::UpdateTickState()
{
	bool bNeedsTick = false;

	for (const FRSAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.Instance && Entry.Instance->IsA<URSRangedWeaponInstance>())
		{
			bNeedsTick = true;
			break;
		}
	}

	// Simulated proxies never need it: nobody aims with another player's pawn, and the spread of
	// a weapon you do not control is not used for anything on this machine.
	const APawn* OwningPawn = GetPawn<APawn>();
	const bool bRelevantHere = OwningPawn && (OwningPawn->IsLocallyControlled() || OwningPawn->HasAuthority());

	SetComponentTickEnabled(bNeedsTick && bRelevantHere);
}

void URSEquipmentManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// Anything equipped before replication was ready still has to be registered as a subobject.
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FRSAppliedEquipmentEntry& Entry : EquipmentList.Entries)
		{
			if (IsValid(Entry.Instance))
			{
				AddReplicatedSubObject(Entry.Instance);
			}
		}
	}
}
