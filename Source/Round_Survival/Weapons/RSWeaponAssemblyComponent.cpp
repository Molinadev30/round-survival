// Round Survival

#include "Weapons/RSWeaponAssemblyComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RSLogChannels.h"
#include "Weapons/RSWeaponAssembly.h"
#include "Weapons/RSWeaponSkin.h"

URSWeaponAssemblyComponent::URSWeaponAssemblyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Cosmetic. An owner that wants its weapon to be found by a trace opts in itself.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(TEXT("NoCollision"));
	bReceivesDecals = false;
}

void URSWeaponAssemblyComponent::SetAssembly(const URSWeaponAssembly* InAssembly)
{
	Assembly = InAssembly;

	SetSkeletalMeshAsset(Assembly ? Assembly->ReceiverMesh.Get() : nullptr);
	RebuildParts();
}

USceneComponent* URSWeaponAssemblyComponent::GetPartComponent(FName PartName) const
{
	const TObjectPtr<USceneComponent>* Found = PartComponents.Find(PartName);
	return Found ? Found->Get() : nullptr;
}

void URSWeaponAssemblyComponent::SetPartHidden(FName PartName, bool bInHidden)
{
	if (USceneComponent* Part = GetPartComponent(PartName))
	{
		// Propagated: a magazine's rounds are attached to it and must go wherever it goes.
		Part->SetHiddenInGame(bInHidden, /*bPropagateToChildren=*/ true);
	}
}

void URSWeaponAssemblyComponent::SetPartRoundCount(FName PartName, int32 Rounds, int32 Capacity)
{
	USkeletalMeshComponent* SkeletalPart = Cast<USkeletalMeshComponent>(GetPartComponent(PartName));
	UAnimSingleNodeInstance* FillInstance = SkeletalPart ? SkeletalPart->GetSingleNodeInstance() : nullptr;
	const UAnimSequenceBase* FillAnimation = FillInstance ? Cast<UAnimSequenceBase>(FillInstance->GetAnimationAsset()) : nullptr;

	if (!FillAnimation || Capacity <= 0)
	{
		return;
	}

	const int32 FrameCount = FillAnimation->GetNumberOfSampledKeys() - 1;
	const float Length = FillAnimation->GetPlayLength();

	if (FrameCount <= 0 || Length <= 0.0f)
	{
		return;
	}

	// One frame per spent round. The depletion animation steps rather than interpolates, and
	// sampling exactly on a key can land on the previous one through float error, so the position
	// sits a quarter of a frame past the key it wants.
	const float SpentFraction = 1.0f - FMath::Clamp(static_cast<float>(Rounds) / Capacity, 0.0f, 1.0f);
	const int32 Frame = FMath::RoundToInt(SpentFraction * FrameCount);
	const float Position = FMath::Min(Length, (Frame + 0.25f) * Length / FrameCount);

	SkeletalPart->SetPosition(Position, /*bFireNotifies=*/ false);
}

void URSWeaponAssemblyComponent::ApplySkin(const URSWeaponSkin* Skin)
{
	URSWeaponSkin::ApplyToComponent(Skin, this);

	if (!Assembly)
	{
		return;
	}

	for (const FRSWeaponPart& Part : Assembly->Parts)
	{
		if (Part.bAcceptsSkin)
		{
			URSWeaponSkin::ApplyToComponent(Skin, Cast<UMeshComponent>(GetPartComponent(Part.PartName)));
		}
	}
}

USceneComponent* URSWeaponAssemblyComponent::FindMuzzle(FName& OutSocket, FVector& OutOffset) const
{
	if (!Assembly)
	{
		OutSocket = NAME_None;
		OutOffset = FVector::ZeroVector;
		return nullptr;
	}

	// Last declared wins: parts are listed in the order they are bolted on.
	for (int32 Index = Assembly->Parts.Num() - 1; Index >= 0; --Index)
	{
		const FRSWeaponPart& Part = Assembly->Parts[Index];
		if (Part.MuzzleSocket.IsNone() && Part.MuzzleOffset.IsZero())
		{
			continue;
		}

		if (USceneComponent* PartComponent = GetPartComponent(Part.PartName))
		{
			OutSocket = Part.MuzzleSocket;
			OutOffset = Part.MuzzleOffset;
			return PartComponent;
		}
	}

	OutSocket = Assembly->ReceiverMuzzleSocket;
	OutOffset = FVector::ZeroVector;
	return const_cast<URSWeaponAssemblyComponent*>(this);
}

void URSWeaponAssemblyComponent::DestroyParts()
{
	for (const TObjectPtr<USceneComponent>& Existing : RepeatedComponents)
	{
		if (Existing)
		{
			Existing->DestroyComponent();
		}
	}

	for (const TPair<FName, TObjectPtr<USceneComponent>>& Existing : PartComponents)
	{
		if (Existing.Value)
		{
			Existing.Value->DestroyComponent();
		}
	}

	RepeatedComponents.Reset();
	PartComponents.Reset();
}

void URSWeaponAssemblyComponent::RebuildParts()
{
	// An owner's construction sets the assembly again on every property change in the editor, so
	// the previous set has to go first or each edit would leave another copy of the weapon behind.
	DestroyParts();

	// The parts are the owner's components, so a template with no actor around it has nowhere to
	// put them.
	AActor* Owner = GetOwner();
	if (!Owner || !Assembly)
	{
		if (Owner && bCentreOnAttachParent)
		{
			// Nothing to centre: an empty display sits on its spot, ready for the next weapon.
			SetRelativeLocation(FVector::ZeroVector);
		}
		return;
	}

	// The parts are derived: whoever owns this sets the assembly again whenever it is constructed or
	// begins play. So they are never saved with a level, nor copied when the level is duplicated for
	// PIE — a locker placed in a level would otherwise carry its editor-built parts into the game,
	// next to the ones BeginPlay builds.
	constexpr EObjectFlags PartFlags = RF_Transient | RF_DuplicateTransient;

	for (const FRSWeaponPart& Part : Assembly->Parts)
	{
		if (Part.PartName.IsNone())
		{
			UE_LOG(LogRS, Warning, TEXT("[%s] has a weapon part with no name; skipped."), *GetNameSafe(Owner));
			continue;
		}

		// Parts are built in order, so a part can hang off one declared before it — the front
		// sight sits on the handguard, not on the receiver.
		USceneComponent* PartParent = this;

		if (!Part.AttachToPart.IsNone())
		{
			if (USceneComponent* ParentPart = GetPartComponent(Part.AttachToPart))
			{
				PartParent = ParentPart;
			}
			else
			{
				UE_LOG(LogRS, Warning, TEXT("[%s] part [%s] wants to attach to [%s], which is not built yet."),
					*GetNameSafe(Owner), *Part.PartName.ToString(), *Part.AttachToPart.ToString());
			}
		}

		USceneComponent* NewPart = nullptr;

		if (Part.SkeletalMesh)
		{
			USkeletalMeshComponent* SkeletalPart = NewObject<USkeletalMeshComponent>(Owner, Part.PartName, PartFlags);
			SkeletalPart->SetSkeletalMeshAsset(Part.SkeletalMesh);

			if (Part.AnimClass && !Part.FillAnimation)
			{
				SkeletalPart->SetAnimInstanceClass(Part.AnimClass);
			}

			NewPart = SkeletalPart;
		}
		else if (Part.StaticMesh)
		{
			UStaticMeshComponent* StaticPart = NewObject<UStaticMeshComponent>(Owner, Part.PartName, PartFlags);
			StaticPart->SetStaticMesh(Part.StaticMesh);
			NewPart = StaticPart;
		}
		else
		{
			UE_LOG(LogRS, Warning, TEXT("[%s] part [%s] has no mesh; skipped."), *GetNameSafe(Owner), *Part.PartName.ToString());
			continue;
		}

		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(NewPart))
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Primitive->SetCollisionProfileName(TEXT("NoCollision"));
			Primitive->bReceivesDecals = false;
			CopyReceiverSettingsTo(Primitive);
		}

		NewPart->SetupAttachment(PartParent, Part.AttachSocket);
		ApplyDefaultHidden(NewPart, Part.bHiddenByDefault);
		NewPart->RegisterComponent();

		PartComponents.Add(Part.PartName, NewPart);

		USkeletalMeshComponent* SkeletalPart = Cast<USkeletalMeshComponent>(NewPart);

		if (SkeletalPart && Part.FillAnimation)
		{
			// Single-node mode with the clock stopped: nothing plays, the part only ever shows the
			// frame SetPartRoundCount puts it on. Starts full, which is also right for a spare.
			SkeletalPart->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			SkeletalPart->SetAnimation(Part.FillAnimation);
			SkeletalPart->Stop();
			SkeletalPart->SetPosition(0.0f, /*bFireNotifies=*/ false);
		}

		if (Part.RepeatedMesh && !Part.RepeatedSocketPrefix.IsEmpty())
		{
			BuildRepeatedMeshes(Part, NewPart);
		}
	}

	// Last, once every part hangs where it will: their positions are what the centre is taken from.
	if (bCentreOnAttachParent)
	{
		CentreOnAttachParent();
	}
}

FBox URSWeaponAssemblyComponent::GetAssembledLocalBounds() const
{
	FBox WeaponBounds(ForceInit);

	if (!Assembly)
	{
		return WeaponBounds;
	}

	if (const USkeletalMesh* Receiver = GetSkeletalMeshAsset())
	{
		WeaponBounds += Receiver->GetImportedBounds().GetBox();
	}

	const FTransform& ReceiverToWorld = GetComponentTransform();

	for (const FRSWeaponPart& Part : Assembly->Parts)
	{
		// A part hidden by default is not part of the weapon on show: the spare magazine is parked
		// below the receiver for the reload, and counting it would centre the weapon on empty air.
		if (Part.bHiddenByDefault)
		{
			continue;
		}

		const USceneComponent* PartComponent = GetPartComponent(Part.PartName);
		if (!PartComponent)
		{
			continue;
		}

		FBox PartBounds(ForceInit);
		if (const UStaticMeshComponent* StaticPart = Cast<UStaticMeshComponent>(PartComponent))
		{
			if (const UStaticMesh* PartStaticMesh = StaticPart->GetStaticMesh())
			{
				PartBounds = PartStaticMesh->GetBoundingBox();
			}
		}
		else if (const USkeletalMeshComponent* SkeletalPart = Cast<USkeletalMeshComponent>(PartComponent))
		{
			if (const USkeletalMesh* PartMesh = SkeletalPart->GetSkeletalMeshAsset())
			{
				PartBounds = PartMesh->GetImportedBounds().GetBox();
			}
		}

		if (PartBounds.IsValid)
		{
			// Parts hang off sockets on the receiver and on each other, so where one sits is read from
			// where it actually is rather than rebuilt from the chain of sockets.
			WeaponBounds += PartBounds.TransformBy(PartComponent->GetComponentTransform().GetRelativeTransform(ReceiverToWorld));
		}
	}

	return WeaponBounds;
}

void URSWeaponAssemblyComponent::CentreOnAttachParent()
{
	const USceneComponent* Parent = GetAttachParent();
	const FBox WeaponBounds = GetAssembledLocalBounds();

	if (!Parent || !WeaponBounds.IsValid)
	{
		return;
	}

	// Worked out in world space so the answer holds whatever the parent is scaled by: a locker's
	// panel is a cube shaped by its scale, and the weapon keeps its real size in front of it.
	const FVector Anchor = Parent->GetSocketLocation(GetAttachSocketName());
	const FVector CentreOffset = GetComponentTransform().TransformVector(WeaponBounds.GetCenter());
	SetWorldLocation(Anchor - CentreOffset);
}

void URSWeaponAssemblyComponent::BuildRepeatedMeshes(const FRSWeaponPart& Part, USceneComponent* PartComponent)
{
	AActor* Owner = GetOwner();

	// Bones count as sockets here, which is the point: the magazine's rounds are bones.
	for (const FName& SocketName : PartComponent->GetAllSocketNames())
	{
		const FString SocketString = SocketName.ToString();
		if (!SocketString.StartsWith(Part.RepeatedSocketPrefix))
		{
			continue;
		}

		const FName ComponentName = MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(),
			FName(*FString::Printf(TEXT("%s_%s"), *Part.PartName.ToString(), *SocketString)));

		// Derived, like the part it hangs on. See RebuildParts.
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Owner, ComponentName, RF_Transient | RF_DuplicateTransient);
		Copy->SetStaticMesh(Part.RepeatedMesh);
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetCollisionProfileName(TEXT("NoCollision"));
		Copy->bReceivesDecals = false;
		Copy->SetCastShadow(false);
		CopyReceiverSettingsTo(Copy);
		Copy->SetupAttachment(PartComponent, SocketName);
		ApplyDefaultHidden(Copy, Part.bHiddenByDefault);
		Copy->RegisterComponent();

		RepeatedComponents.Add(Copy);
	}
}

void URSWeaponAssemblyComponent::CopyReceiverSettingsTo(UPrimitiveComponent* Part) const
{
	Part->SetOnlyOwnerSee(bOnlyOwnerSee);
	Part->SetOwnerNoSee(bOwnerNoSee);
	Part->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);

	if (USkeletalMeshComponent* SkeletalPart = Cast<USkeletalMeshComponent>(Part))
	{
		SkeletalPart->PrimaryComponentTick.bCanEverTick = PrimaryComponentTick.bCanEverTick;
		SkeletalPart->bNoSkeletonUpdate = bNoSkeletonUpdate;
		SkeletalPart->VisibilityBasedAnimTickOption = VisibilityBasedAnimTickOption;
	}
}

void URSWeaponAssemblyComponent::ApplyDefaultHidden(USceneComponent* Part, bool bHiddenByDefault) const
{
	Part->SetHiddenInGame(bHiddenByDefault);

	const UWorld* World = GetWorld();
	if (bHiddenByDefault && World && !World->IsGameWorld())
	{
		Part->SetVisibility(false);
	}
}
