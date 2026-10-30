// Round Survival

#include "Enemies/RSDismembermentComponent.h"

#include "AbilitySystem/Attributes/RSHealthSet.h"
#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemies/RSDismembermentProfile.h"
#include "Enemies/RSEnemyDefinition.h"
#include "Enemies/RSSeveredPiece.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "PhysicsEngine/BodySetup.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

namespace RSDismemberment
{
	/**
	 * What a part that has come off still weighs, as a share of its mass. Its bodies stay and keep
	 * simulating with the ragdoll, so the bones they drive stay on the body; this keeps a limb
	 * nobody can see from dragging the corpse.
	 */
	static constexpr float SeveredMassScale = 0.01f;

	/** How recently a body must have been drawn on this machine for a cut on it to be shown. */
	static constexpr float RecentlyRenderedWindow = 0.2f;

	/**
	 * Switches the bodies of a part — its root bone's and everything below — between what the
	 * physics asset gives them and nothing.
	 *
	 * Shape by shape, because on a skeletal mesh a body's own collision setting is ignored in
	 * favour of the component's; a shape's is intersected with it. The bodies themselves are kept:
	 * a body destroyed outright leaves its bone at the world origin once the ragdoll starts, since
	 * the physics blend reads the bone from the body it still maps to.
	 */
	static void SetPartBodiesActive(USkeletalMeshComponent* Body, FName RootBone, bool bActive)
	{
		Body->ForEachBodyBelow(RootBone, /*bIncludeSelf=*/ true, /*bSkipCustomType=*/ false, [bActive](FBodyInstance* Instance)
		{
			UBodySetup* Setup = Instance->GetBodySetup();
			if (!Setup)
			{
				return;
			}

			for (int32 ShapeIndex = 0; ShapeIndex < Setup->AggGeom.GetElementCount(); ++ShapeIndex)
			{
				const ECollisionEnabled::Type Default = Setup->AggGeom.GetElement(ShapeIndex)->GetCollisionEnabled();
				Instance->SetShapeCollisionEnabled(ShapeIndex, bActive ? Default : ECollisionEnabled::NoCollision);
			}

			Instance->SetMassScale(bActive ? Setup->DefaultInstance.MassScale : SeveredMassScale);
		});
	}
}

URSDismembermentComponent::URSDismembermentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	// Everything here answers a hit or a replicated change; nothing waits on a frame.
	PrimaryComponentTick.bCanEverTick = false;
}

FGameplayTag URSDismembermentComponent::GetPartTag(ERSBodyPart Part)
{
	switch (Part)
	{
	case ERSBodyPart::Head:		return RSGameplayTags::BodyPart_Head;
	case ERSBodyPart::ArmLeft:	return RSGameplayTags::BodyPart_ArmLeft;
	case ERSBodyPart::ArmRight:	return RSGameplayTags::BodyPart_ArmRight;
	case ERSBodyPart::LegLeft:	return RSGameplayTags::BodyPart_LegLeft;
	case ERSBodyPart::LegRight:	return RSGameplayTags::BodyPart_LegRight;
	default:					return FGameplayTag();
	}
}

void URSDismembermentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, State);
}

const URSDismembermentProfile* URSDismembermentComponent::GetProfile() const
{
	const ARSZombieCharacter* Enemy = GetOwner<ARSZombieCharacter>();
	const URSEnemyDefinition* Definition = Enemy ? Enemy->GetEnemyDefinition() : nullptr;
	return Definition ? Definition->Dismemberment.Get() : nullptr;
}

void URSDismembermentComponent::HandleHealthChanged(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
	float Magnitude, float OldValue, float NewValue)
{
	// Healing, and the client's echo of a change the server already charged.
	if (!GetOwner()->HasAuthority() || Spec == nullptr || NewValue >= OldValue)
	{
		return;
	}

	const URSDismembermentProfile* Profile = GetProfile();
	const FHitResult* Hit = Spec->GetContext().GetHitResult();
	if (!Profile || !Hit || Hit->BoneName.IsNone())
	{
		// Damage that landed nowhere in particular — the debug wipe, Purge — takes nothing off.
		return;
	}

	const ERSBodyPart Part = FindPartForBone(Hit->BoneName);
	if (Part == ERSBodyPart::MAX || IsSevered(Part))
	{
		UE_LOG(LogRSAbility, VeryVerbose, TEXT("[%s] hit on %s: no part to charge."), *GetNameSafe(GetOwner()), *Hit->BoneName.ToString());
		return;
	}

	const FRSBodyPartRule* Rule = Profile->FindRule(Part);
	check(Rule);

	// The magnitude, not the drop: the health set clamps at zero, and the hit that kills would
	// otherwise be charged only what was left of the enemy, however hard it landed.
	PartDamage[static_cast<int32>(Part)] += Magnitude;

	const bool bKillingBlow = NewValue <= 0.0f;
	const ARSZombieCharacter* Enemy = GetOwner<ARSZombieCharacter>();
	const float Threshold = Rule->SeverThreshold * (Enemy ? Enemy->GetMaxHealth() : 0.0f);

	UE_LOG(LogRSAbility, VeryVerbose, TEXT("[%s] hit on %s: %s charged %.1f, %.1f of %.1f%s."),
		*GetNameSafe(GetOwner()), *Hit->BoneName.ToString(), *UEnum::GetValueAsString(Part), Magnitude,
		PartDamage[static_cast<int32>(Part)], Threshold, bKillingBlow ? TEXT(", killing") : TEXT(""));

	if (Rule->bOnlyOnKillingBlow && !bKillingBlow)
	{
		return;
	}

	if (PartDamage[static_cast<int32>(Part)] >= Threshold)
	{
		// The way the blow travelled: along the trace that landed it.
		FVector CutDirection = (Hit->TraceEnd - Hit->TraceStart).GetSafeNormal();
		if (CutDirection.IsNearlyZero())
		{
			CutDirection = -Hit->ImpactNormal;
		}

		SeverPart(Part, EffectInstigator, CutDirection);
	}
}

ERSBodyPart URSDismembermentComponent::FindPartForBone(FName BoneName) const
{
	const URSDismembermentProfile* Profile = GetProfile();
	const ACharacter* Character = GetOwner<ACharacter>();
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	if (!Profile || !Asset)
	{
		return ERSBodyPart::MAX;
	}

	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	for (int32 BoneIndex = Skeleton.FindBoneIndex(BoneName); BoneIndex != INDEX_NONE; BoneIndex = Skeleton.GetParentIndex(BoneIndex))
	{
		const FName Name = Skeleton.GetBoneName(BoneIndex);
		for (const FRSBodyPartRule& Rule : Profile->Parts)
		{
			if (Rule.RootBone == Name)
			{
				return Rule.Part;
			}
		}
	}

	// The torso, or a bone above every part's root.
	return ERSBodyPart::MAX;
}

bool URSDismembermentComponent::SeverPart(ERSBodyPart Part, AActor* Instigator, const FVector& CutDirection)
{
	if (!GetOwner()->HasAuthority() || Part == ERSBodyPart::MAX || IsSevered(Part))
	{
		return false;
	}

	const uint8 PreviousMask = State.SeveredMask;
	State.SeveredMask |= PartBit(Part);
	State.LastCutDirection = CutDirection.GetSafeNormal();

	const ARSZombieCharacter* Enemy = GetOwner<ARSZombieCharacter>();
	URSAbilitySystemComponent* ASC = Enemy ? Enemy->GetRSAbilitySystemComponent() : nullptr;
	const URSDismembermentProfile* Profile = GetProfile();
	const FRSBodyPartRule* Rule = Profile ? Profile->FindRule(Part) : nullptr;

	if (ASC)
	{
		// What the loss does, before anyone is told of it: whatever answers the event finds the
		// enemy already changed.
		if (Rule && Rule->SeverEffect)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(Instigator, Instigator);
			ASC->ApplyGameplayEffectToSelf(Rule->SeverEffect->GetDefaultObject<UGameplayEffect>(), 1.0f, Context);
		}

		FGameplayEventData EventData;
		EventData.EventTag = RSGameplayTags::Event_Dismembered;
		EventData.Instigator = Instigator;
		EventData.Target = GetOwner();
		EventData.TargetTags.AddTag(GetPartTag(Part));
		ASC->HandleGameplayEvent(RSGameplayTags::Event_Dismembered, &EventData);
	}

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] lost %s (instigator [%s])."),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Part), *GetNameSafe(Instigator));

	// OnRep never runs on the server, and on a listen server the host draws these bodies too.
	HandleStateChanged(PreviousMask);
	return true;
}

void URSDismembermentComponent::ResetForNewLife()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	FMemory::Memzero(PartDamage);

	const uint8 PreviousMask = State.SeveredMask;
	State = FRSDismemberState();
	HandleStateChanged(PreviousMask);
}

void URSDismembermentComponent::OnRep_State(const FRSDismemberState& PreviousState)
{
	HandleStateChanged(PreviousState.SeveredMask);
}

void URSDismembermentComponent::HandleStateChanged(uint8 PreviousMask)
{
	if (PreviousMask == State.SeveredMask)
	{
		return;
	}

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] severed mask 0x%02x -> 0x%02x (%s)."),
		*GetNameSafe(GetOwner()), PreviousMask, State.SeveredMask,
		GetOwner()->HasAuthority() ? TEXT("server") : TEXT("client"));

	// Something came back on, which only a new life does — and the new life may already have lost
	// something of its own by the time a client hears of it. Rebuilt from whole.
	if ((PreviousMask & ~State.SeveredMask) != 0)
	{
		RestoreWholeBody(PreviousMask);
		PreviousMask = 0;
	}

	const uint8 NewlySevered = State.SeveredMask & ~PreviousMask;
	if (NewlySevered == 0)
	{
		return;
	}

	// The cut first, while the body still has the pose the part had on it.
	if (ShouldPlayCut())
	{
		for (ERSBodyPart Part : TEnumRange<ERSBodyPart>())
		{
			if ((NewlySevered & PartBit(Part)) != 0)
			{
				PlayCut(Part);
			}
		}
	}

	TakeApart(NewlySevered);
}

bool URSDismembermentComponent::ShouldPlayCut() const
{
	const AActor* Owner = GetOwner();
	const ACharacter* Character = Cast<ACharacter>(Owner);
	if (!Character || Owner->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// A client receives the state of an enemy it has only just started to receive before that
	// enemy begins play. Those parts came off before it was watching: drawn gone, not thrown.
	if (!Owner->HasActorBegunPlay())
	{
		return false;
	}

	return Character->GetMesh()->WasRecentlyRendered(RSDismemberment::RecentlyRenderedWindow);
}

void URSDismembermentComponent::PlayCut(ERSBodyPart Part)
{
	ACharacter* Character = GetOwner<ACharacter>();
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	const URSDismembermentProfile* Profile = GetProfile();
	const FRSBodyPartRule* Rule = Profile ? Profile->FindRule(Part) : nullptr;
	if (!Body || !Rule || Body->GetBoneIndex(Rule->RootBone) == INDEX_NONE)
	{
		return;
	}

	const FVector CutLocation = Body->GetSocketLocation(Rule->RootBone);
	const FVector CutDirection = State.LastCutDirection;

	if (Rule->CutSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Rule->CutSound, CutLocation);
	}

	if (Rule->CutEffect)
	{
		const FRotator Facing = CutDirection.IsNearlyZero() ? FRotator::ZeroRotator : CutDirection.Rotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Rule->CutEffect, CutLocation, Facing);
	}

	// On the bone the part hung from, at the joint, pointing out of the stump. Not on the part's
	// own root: that bone stays with the body, but it swings with an arm nobody can see.
	const FName StumpBone = Body->GetParentBone(Rule->RootBone);
	if (Rule->StumpEffect && !StumpBone.IsNone())
	{
		const FVector Outward = (CutLocation - Body->GetSocketLocation(StumpBone)).GetSafeNormal();
		UNiagaraComponent* Stump = UNiagaraFunctionLibrary::SpawnSystemAttached(Rule->StumpEffect, Body, StumpBone,
			CutLocation, Outward.Rotation(), EAttachLocation::KeepWorldPosition, /*bAutoDestroy=*/ true);

		// The artery effects loop: switched off after the duration, then gone on their own.
		if (Stump && Rule->StumpEffectDuration > 0.0f)
		{
			FTimerHandle Handle;
			GetWorld()->GetTimerManager().SetTimer(Handle,
				FTimerDelegate::CreateWeakLambda(Stump, [Stump]() { Stump->Deactivate(); }),
				Rule->StumpEffectDuration, /*bLoop=*/ false);
		}
	}

	const FRSDismemberedMeshSet* Set = GetMeshSet();
	const TObjectPtr<USkeletalMesh>* PieceMesh = Set ? Set->PartMeshes.Find(Part) : nullptr;
	URSSeveredPieceSubsystem* Pieces = URSSeveredPieceSubsystem::Get(this);
	if (PieceMesh && *PieceMesh && Pieces)
	{
		const FVector Velocity = Character->GetVelocity() + CutDirection * Rule->PieceLaunchSpeed + FVector::UpVector * Rule->PieceLift;
		Pieces->Launch(*PieceMesh, *Body, Velocity, Rule->PieceEffect);
	}
}

const FRSDismemberedMeshSet* URSDismembermentComponent::GetMeshSet() const
{
	const URSDismembermentProfile* Profile = GetProfile();
	const ACharacter* Character = GetOwner<ACharacter>();
	const USkeletalMesh* Whole = WholeMesh ? WholeMesh.Get() : (Character ? Character->GetMesh()->GetSkeletalMeshAsset() : nullptr);
	return Profile && Whole ? Profile->FindMeshSet(Whole) : nullptr;
}

void URSDismembermentComponent::TakeApart(uint8 NewlySevered)
{
	ACharacter* Character = GetOwner<ACharacter>();
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	const URSDismembermentProfile* Profile = GetProfile();
	if (!Body || !Profile)
	{
		return;
	}

	const FRSDismemberedMeshSet* Set = GetMeshSet();
	if (Set && Set->TorsoWithHead && Set->TorsoWithoutHead)
	{
		if (!WholeMesh)
		{
			WholeMesh = Body->GetSkeletalMeshAsset();
		}

		// The torso takes over as the body that animates, simulates and is hit. Same skeleton, and
		// the pose is not reset, so the animation instance carries on where it was — a swing or a
		// vault the cut lands in finishes. The physics asset is the class's override rather than
		// the mesh's, so its bodies are only mapped onto the new mesh's bones, not rebuilt.
		USkeletalMesh* Torso = IsSevered(ERSBodyPart::Head) ? Set->TorsoWithoutHead : Set->TorsoWithHead;
		Body->SetSkeletalMesh(Torso, /*bReinitPose=*/ false);

		for (ERSBodyPart Part : TEnumRange<ERSBodyPart>())
		{
			// The head is part of the torso while it is on.
			const TObjectPtr<USkeletalMesh>* PartMesh = Set->PartMeshes.Find(Part);
			if (Part == ERSBodyPart::Head || !PartMesh || !*PartMesh)
			{
				continue;
			}

			const bool bAttached = !IsSevered(Part);
			const int32 Index = static_cast<int32>(Part);
			USkeletalMeshComponent* Piece = bAttached
				? FindOrAddPartComponent(Part, *PartMesh)
				: (PartComponents.IsValidIndex(Index) ? PartComponents[Index].Get() : nullptr);
			if (Piece)
			{
				SetPartWorn(Piece, bAttached);
			}
		}
	}
	else
	{
		UE_LOG(LogRSAbility, Warning, TEXT("[%s] lost a part, but [%s] has no pieces for [%s]: it is drawn whole."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Profile), *GetNameSafe(Body->GetSkeletalMeshAsset()));
	}

	// What has gone stops being hit, whether or not it could be drawn gone: its bodies lose their
	// collision and nearly all their mass. They are kept, and simulate with the rest when the body
	// falls, so the bones they drive — which the stump's skin follows — stay where the limb was.
	for (ERSBodyPart Part : TEnumRange<ERSBodyPart>())
	{
		const FRSBodyPartRule* Rule = (NewlySevered & PartBit(Part)) != 0 ? Profile->FindRule(Part) : nullptr;
		if (Rule)
		{
			RSDismemberment::SetPartBodiesActive(Body, Rule->RootBone, /*bActive=*/ false);
		}
	}
}

USkeletalMeshComponent* URSDismembermentComponent::FindOrAddPartComponent(ERSBodyPart Part, USkeletalMesh* PartMesh)
{
	const int32 Index = static_cast<int32>(Part);
	PartComponents.SetNum(static_cast<int32>(ERSBodyPart::MAX));

	if (USkeletalMeshComponent* Existing = PartComponents[Index])
	{
		Existing->SetSkeletalMesh(PartMesh);
		return Existing;
	}

	ACharacter* Character = GetOwner<ACharacter>();
	USkeletalMeshComponent* Body = Character->GetMesh();

	const FName Name(*FString::Printf(TEXT("Dismember_%s"), *StaticEnum<ERSBodyPart>()->GetNameStringByValue(Index)));
	USkeletalMeshComponent* Piece = NewObject<USkeletalMeshComponent>(Character, Name);
	Piece->SetupAttachment(Body);
	Piece->SetSkeletalMesh(PartMesh);

	// Only drawn. Shots land on the torso's physics asset, which has the limb's bodies for as long
	// as the limb is on.
	Piece->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetCanEverAffectNavigation(false);
	Piece->bReceivesDecals = false;
	Piece->bUseBoundsFromLeaderPoseComponent = true;

	PartComponents[Index] = Piece;
	return Piece;
}

void URSDismembermentComponent::SetPartWorn(USkeletalMeshComponent* Piece, bool bWorn)
{
	if (!bWorn)
	{
		// Hidden but registered, it would still be marked dirty by the torso every frame and pay
		// for its transform and bounds at the end of it. Unregistered it costs nothing, and keeps
		// its attachment for the next time it is worn.
		if (Piece->IsRegistered())
		{
			Piece->UnregisterComponent();
		}
		return;
	}

	if (!Piece->IsRegistered())
	{
		Piece->RegisterComponent();

		// The torso poses it, so it has nothing of its own to tick. Registering activates it,
		// which turns its tick back on, and its bone map is rebuilt against the torso it now has.
		Piece->SetLeaderPoseComponent(GetOwner<ACharacter>()->GetMesh(), /*bForceUpdate=*/ true);
		Piece->SetComponentTickEnabled(false);
	}
}

void URSDismembermentComponent::RestoreWholeBody(uint8 SeveredMask)
{
	ACharacter* Character = GetOwner<ACharacter>();
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Body)
	{
		return;
	}

	for (USkeletalMeshComponent* Piece : PartComponents)
	{
		if (Piece)
		{
			SetPartWorn(Piece, false);
		}
	}

	if (WholeMesh)
	{
		Body->SetSkeletalMesh(WholeMesh, /*bReinitPose=*/ false);
		WholeMesh = nullptr;
	}

	// The bodies of what had gone, back as the physics asset has them.
	if (const URSDismembermentProfile* Profile = GetProfile())
	{
		for (ERSBodyPart Part : TEnumRange<ERSBodyPart>())
		{
			const FRSBodyPartRule* Rule = (SeveredMask & PartBit(Part)) != 0 ? Profile->FindRule(Part) : nullptr;
			if (Rule)
			{
				RSDismemberment::SetPartBodiesActive(Body, Rule->RootBone, /*bActive=*/ true);
			}
		}
	}
}

FString URSDismembermentComponent::GetDebugString() const
{
	const URSDismembermentProfile* Profile = GetProfile();
	const ARSZombieCharacter* Enemy = GetOwner<ARSZombieCharacter>();
	const float MaxHealth = Enemy ? Enemy->GetMaxHealth() : 0.0f;
	if (!Profile || MaxHealth <= 0.0f)
	{
		return FString();
	}

	// One entry per part: gone, or how much of its threshold it has taken.
	FString Result;
	for (const FRSBodyPartRule& Rule : Profile->Parts)
	{
		const FString PartName = StaticEnum<ERSBodyPart>()->GetNameStringByValue(static_cast<int64>(Rule.Part));
		if (IsSevered(Rule.Part))
		{
			Result += FString::Printf(TEXT("%s OFF  "), *PartName);
			continue;
		}

		const float Threshold = Rule.SeverThreshold * MaxHealth;
		const float Taken = PartDamage[static_cast<int32>(Rule.Part)];
		if (Taken > 0.0f)
		{
			Result += Threshold > 0.0f
				? FString::Printf(TEXT("%s %.0f%%  "), *PartName, 100.0f * Taken / Threshold)
				: FString::Printf(TEXT("%s hit  "), *PartName);
		}
	}

	return Result.TrimEnd();
}
