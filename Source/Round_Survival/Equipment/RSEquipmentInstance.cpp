// Round Survival

#include "Equipment/RSEquipmentInstance.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/RSPlayerCharacter.h"
#include "Engine/World.h"
#include "Equipment/RSEquipmentDefinition.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "RSLogChannels.h"

URSEquipmentInstance::URSEquipmentInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWorld* URSEquipmentInstance::GetWorld() const
{
	APawn* OwningPawn = GetPawn();
	return OwningPawn ? OwningPawn->GetWorld() : nullptr;
}

void URSEquipmentInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Instigator);
}

void URSEquipmentInstance::OnRep_Instigator()
{
	OnInstigatorChanged();
}

APawn* URSEquipmentInstance::GetPawn() const
{
	return Cast<APawn>(GetOuter());
}

APawn* URSEquipmentInstance::GetTypedPawn(TSubclassOf<APawn> PawnType) const
{
	if (UClass* ActualPawnType = PawnType)
	{
		if (GetOuter()->IsA(ActualPawnType))
		{
			return Cast<APawn>(GetOuter());
		}
	}

	return nullptr;
}

TArray<AActor*> URSEquipmentInstance::GetSpawnedActors() const
{
	TArray<AActor*> Result;
	Result.Reserve(SpawnedActors.Num());

	for (const FRSSpawnedEquipmentActor& Spawned : SpawnedActors)
	{
		Result.Add(Spawned.Actor);
	}

	return Result;
}

AActor* URSEquipmentInstance::GetSpawnedActor(bool bFirstPerson) const
{
	for (const FRSSpawnedEquipmentActor& Spawned : SpawnedActors)
	{
		if (Spawned.bFirstPerson == bFirstPerson)
		{
			return Spawned.Actor;
		}
	}

	return nullptr;
}

void URSEquipmentInstance::SpawnEquipmentActors(const TArray<FRSEquipmentActorToSpawn>& ActorsToSpawn)
{
	APawn* OwningPawn = GetPawn();
	if (!OwningPawn)
	{
		return;
	}

	UWorld* World = OwningPawn->GetWorld();
	if (!World)
	{
		return;
	}

	ACharacter* OwningCharacter = Cast<ACharacter>(OwningPawn);
	ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(OwningPawn);

	// Nothing is drawn on a dedicated server, so nobody there looks through anyone's eyes.
	const bool bAnyoneCanLookThroughEyes = World->GetNetMode() != NM_DedicatedServer;

	for (const FRSEquipmentActorToSpawn& SpawnInfo : ActorsToSpawn)
	{
		if (!SpawnInfo.ActorToSpawn)
		{
			continue;
		}

		// The viewmodel is spawned wherever someone might look through this pawn's eyes: the
		// player, and any dead teammate watching them in first person. The renderer only draws it
		// for a view through those eyes, so it costs nothing to be seen by nobody.
		if (SpawnInfo.bFirstPerson && !bAnyoneCanLookThroughEyes)
		{
			continue;
		}

		USceneComponent* AttachTarget = OwningPawn->GetRootComponent();

		if (SpawnInfo.bFirstPerson)
		{
			if (!OwningPlayer)
			{
				UE_LOG(LogRS, Warning, TEXT("[%s] wants a first-person actor but [%s] has no viewmodel mesh."),
					*GetNameSafe(this), *GetNameSafe(OwningPawn));
				continue;
			}

			AttachTarget = OwningPlayer->GetFirstPersonMesh();
		}
		else if (OwningCharacter)
		{
			AttachTarget = OwningCharacter->GetMesh();
		}

		AActor* NewActor = World->SpawnActorDeferred<AActor>(SpawnInfo.ActorToSpawn, FTransform::Identity, OwningPawn);
		if (!NewActor)
		{
			continue;
		}

		// These actors are cosmetic and every machine spawns its own, so replicating them would
		// send the same weapon twice. All equipment state lives on this instance instead.
		NewActor->SetReplicates(false);
		NewActor->FinishSpawning(FTransform::Identity, /*bIsDefaultTransform=*/ true);
		NewActor->SetActorRelativeTransform(SpawnInfo.AttachTransform);
		NewActor->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform, SpawnInfo.AttachSocket);

		// A view through the pawn's eyes sees the viewmodel and not the body weapon; every other
		// view sees the opposite. "Owner" is the renderer's word for the first: the actor whose
		// eyes the view is from (see URSLocalPlayer). The rendering mode is set here rather than in
		// the Blueprint so that one weapon actor asset serves both copies — the only difference
		// between them is which side of the camera they are on, and that is something only the
		// spawner knows.
		const bool bFirstPerson = SpawnInfo.bFirstPerson;
		NewActor->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors=*/ true,
			[bFirstPerson](UPrimitiveComponent* Primitive)
			{
				Primitive->SetOnlyOwnerSee(bFirstPerson);
				Primitive->SetOwnerNoSee(!bFirstPerson);
				Primitive->SetFirstPersonPrimitiveType(bFirstPerson
					? EFirstPersonPrimitiveType::FirstPerson
					: EFirstPersonPrimitiveType::WorldSpaceRepresentation);

				// As the arms it hangs from: unseen, it only keeps its montages going.
				if (USkinnedMeshComponent* SkinnedMesh = Cast<USkinnedMeshComponent>(Primitive); SkinnedMesh && bFirstPerson)
				{
					SkinnedMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
				}
			});

		FRSSpawnedEquipmentActor& Spawned = SpawnedActors.AddDefaulted_GetRef();
		Spawned.Actor = NewActor;
		Spawned.bFirstPerson = SpawnInfo.bFirstPerson;
	}
}

void URSEquipmentInstance::DestroyEquipmentActors()
{
	for (const FRSSpawnedEquipmentActor& Spawned : SpawnedActors)
	{
		if (Spawned.Actor)
		{
			Spawned.Actor->Destroy();
		}
	}

	SpawnedActors.Reset();
}

void URSEquipmentInstance::OnEquipped()
{
	if (EquipmentDefinition)
	{
		SpawnEquipmentActors(GetDefault<URSEquipmentDefinition>(EquipmentDefinition)->ActorsToSpawn);
	}

	K2_OnEquipped();
}

void URSEquipmentInstance::OnUnequipped()
{
	DestroyEquipmentActors();

	K2_OnUnequipped();
}
