// Round Survival

#include "Equipment/RSWeaponInstance.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/RSAnimInstance.h"
#include "Animation/RSViewmodelAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/RSPlayerCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RSGameplayTags.h"
#include "TimerManager.h"
#include "Weapons/RSMeleeStrikeSet.h"
#include "Weapons/RSWeaponActor.h"
#include "Weapons/RSWeaponAssembly.h"
#include "Weapons/RSWeaponAssemblyComponent.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponItemInstance.h"

URSWeaponInstance::URSWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URSWeaponInstance::OnEquipped()
{
	// Super spawns the weapon actors, empty: what they look like comes from the item.
	Super::OnEquipped();

	if (const UWorld* World = GetWorld())
	{
		TimeLastEquipped = World->GetTimeSeconds();
	}

	// The item may already be known — always on the server, often on a client — or may arrive later
	// through OnInstigatorChanged. Either way the weapon is assembled, its magazine posed and its
	// finish put on as soon as there is an item to read them from. The assembly goes first: it is
	// what gives the weapon copies a mesh for everything after it to work on.
	BindToItem();
	RefreshAssembly();
	RefreshLoadedMagazine();
	RefreshSkin();

	ApplyAnimLayers();

	// The grip is the assembly's, so it follows the item too, and so does the sight.
	ApplyLeftHandGrip();
	ApplyAimDownSightsOffset();

	// The draw plays here rather than in the switch ability, so a weapon comes up the same way
	// whatever put it in the player's hands, and every machine plays its own copy.
	PlayWeaponMontagePair(FirstPersonWeaponEquipMontage, ThirdPersonWeaponEquipMontage, 1.0f);
	PlayMontagePair(FirstPersonEquipMontage, ThirdPersonEquipMontage, 1.0f);
}

void URSWeaponInstance::OnInstigatorChanged()
{
	Super::OnInstigatorChanged();

	BindToItem();
	RefreshAssembly();
	RefreshLoadedMagazine();
	RefreshSkin();
	ApplyLeftHandGrip();
	ApplyAimDownSightsOffset();
}

URSWeaponItemInstance* URSWeaponInstance::GetItem() const
{
	return Cast<URSWeaponItemInstance>(GetInstigator());
}

int32 URSWeaponInstance::GetItemStatTagStackCount(FGameplayTag Tag) const
{
	const URSWeaponItemInstance* Item = GetItem();
	return Item ? Item->GetStatTagStackCount(Tag) : 0;
}

void URSWeaponInstance::BindToItem()
{
	URSWeaponItemInstance* Item = GetItem();
	if (!Item || BoundItem.Get() == Item)
	{
		return;
	}

	UnbindFromItem();

	BoundItem = Item;
	ItemStatsChangedHandle = Item->OnStatsChanged.AddUObject(this, &ThisClass::HandleItemStatsChanged);
	ItemSkinChangedHandle = Item->OnSkinChanged.AddUObject(this, &ThisClass::HandleItemSkinChanged);
}

void URSWeaponInstance::UnbindFromItem()
{
	if (URSWeaponItemInstance* Item = BoundItem.Get())
	{
		Item->OnStatsChanged.Remove(ItemStatsChangedHandle);
		Item->OnSkinChanged.Remove(ItemSkinChangedHandle);
	}

	BoundItem.Reset();
	ItemStatsChangedHandle.Reset();
	ItemSkinChangedHandle.Reset();
}

void URSWeaponInstance::HandleItemStatsChanged(URSWeaponItemInstance* Item)
{
	RefreshLoadedMagazine();
}

void URSWeaponInstance::HandleItemSkinChanged(URSWeaponItemInstance* Item)
{
	RefreshSkin();
}

ERSLeftHandGrip URSWeaponInstance::GetLeftHandGrip() const
{
	// From the data, not from the weapon copies: the answer is the same on every machine, the server
	// included, whether or not anything has been built.
	const URSWeaponAssembly* Assembly = GetItemAssembly();
	return Assembly ? Assembly->GetLeftHandGrip() : ERSLeftHandGrip::Handguard;
}

void URSWeaponInstance::ApplyLeftHandGrip()
{
	const ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	if (!OwningPlayer)
	{
		return;
	}

	const FRSLeftHandGripPoses* Poses = LeftHandGripPoses.Find(GetLeftHandGrip());

	// The viewmodel only exists where the pawn is controlled from; elsewhere this half finds no
	// mesh and does nothing, as the montage pairs do.
	if (const USkeletalMeshComponent* ViewmodelMesh = OwningPlayer->GetFirstPersonMesh())
	{
		if (URSAnimInstance* ViewmodelAnim = Cast<URSAnimInstance>(ViewmodelMesh->GetAnimInstance()))
		{
			ViewmodelAnim->SetLeftHandGripPoses(Poses ? Poses->FirstPersonIdle : nullptr, Poses ? Poses->FirstPersonAim : nullptr);
		}
	}

	if (const USkeletalMeshComponent* BodyMesh = OwningPlayer->GetMesh())
	{
		if (URSAnimInstance* BodyAnim = Cast<URSAnimInstance>(BodyMesh->GetAnimInstance()))
		{
			BodyAnim->SetLeftHandGripPoses(Poses ? Poses->ThirdPersonIdle : nullptr, Poses ? Poses->ThirdPersonAim : nullptr);
		}
	}
}

void URSWeaponInstance::ClearLeftHandGrip()
{
	const ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	if (!OwningPlayer)
	{
		return;
	}

	for (const USkeletalMeshComponent* Mesh : { OwningPlayer->GetFirstPersonMesh(), OwningPlayer->GetMesh() })
	{
		if (URSAnimInstance* AnimInstance = Mesh ? Cast<URSAnimInstance>(Mesh->GetAnimInstance()) : nullptr)
		{
			AnimInstance->SetLeftHandGripPoses(nullptr, nullptr);
		}
	}
}

void URSWeaponInstance::ApplyAimDownSightsOffset()
{
	const ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	const URSWeaponAssembly* Assembly = GetItemAssembly();
	if (!OwningPlayer || !Assembly)
	{
		return;
	}

	// First person only: the body's aimed pose is the third-person animation's own.
	if (const USkeletalMeshComponent* ViewmodelMesh = OwningPlayer->GetFirstPersonMesh())
	{
		if (URSViewmodelAnimInstance* ViewmodelAnim = Cast<URSViewmodelAnimInstance>(ViewmodelMesh->GetAnimInstance()))
		{
			ViewmodelAnim->SetAimDownSightsOffset(Assembly->AimDownSightsOffset);
		}
	}
}

void URSWeaponInstance::RefreshAssembly()
{
	const URSWeaponAssembly* Assembly = GetItemAssembly();

	for (const bool bFirstPerson : { true, false })
	{
		URSWeaponAssemblyComponent* WeaponAssembly = GetWeaponAssembly(bFirstPerson);

		// Once per weapon copy: the item does not change its assembly, and rebuilding would undo
		// the magazine and the skin that were put on after it.
		if (WeaponAssembly && WeaponAssembly->GetAssembly() != Assembly)
		{
			WeaponAssembly->SetAssembly(Assembly);
		}
	}
}

const URSWeaponAssembly* URSWeaponInstance::GetItemAssembly() const
{
	const URSWeaponItemInstance* Item = GetItem();
	const URSWeaponItemDefinition* Definition = Item ? Item->GetDefinition() : nullptr;
	return Definition ? Definition->Assembly.Get() : nullptr;
}

void URSWeaponInstance::RefreshSkin()
{
	// Null until the item is known on this machine, which leaves the mesh's own finish on for the few
	// frames before it arrives rather than guessing.
	const URSWeaponItemInstance* Item = GetItem();
	const URSWeaponSkin* Skin = Item ? Item->GetSkin() : nullptr;

	for (const bool bFirstPerson : { true, false })
	{
		if (URSWeaponAssemblyComponent* Assembly = GetWeaponAssembly(bFirstPerson))
		{
			Assembly->ApplySkin(Skin);
		}
	}
}

void URSWeaponInstance::RefreshLoadedMagazine()
{
	if (LoadedMagazinePart.IsNone())
	{
		return;
	}

	const int32 Rounds = GetItemStatTagStackCount(RSGameplayTags::Weapon_Ammo_Magazine);
	const int32 Capacity = GetItemStatTagStackCount(RSGameplayTags::Weapon_MagazineSize);

	for (const bool bFirstPerson : { true, false })
	{
		if (URSWeaponAssemblyComponent* Assembly = GetWeaponAssembly(bFirstPerson))
		{
			Assembly->SetPartRoundCount(LoadedMagazinePart, Rounds, Capacity);
		}
	}
}

void URSWeaponInstance::ShowSpareMagazineForReload()
{
	if (SpareMagazinePart.IsNone())
	{
		return;
	}

	const int32 Capacity = GetItemStatTagStackCount(RSGameplayTags::Weapon_MagazineSize);
	const int32 Magazine = GetItemStatTagStackCount(RSGameplayTags::Weapon_Ammo_Magazine);
	const int32 Reserve = GetItemStatTagStackCount(RSGameplayTags::Weapon_Ammo_Reserve);
	const int32 Rounds = FMath::Min(Capacity, Magazine + Reserve);

	for (const bool bFirstPerson : { true, false })
	{
		if (URSWeaponAssemblyComponent* Assembly = GetWeaponAssembly(bFirstPerson))
		{
			Assembly->SetPartRoundCount(SpareMagazinePart, Rounds, Capacity);
		}
	}
}

void URSWeaponInstance::OnUnequipped()
{
	// Before Super: it destroys the actors this needs to reach, and the character graphs have to
	// be put back the way they were found or the next weapon inherits this one's pose.
	ClearAnimLayers();
	ClearLeftHandGrip();
	UnbindFromItem();

	Super::OnUnequipped();
}

USkeletalMeshComponent* URSWeaponInstance::GetWeaponMeshComponent(bool bFirstPerson) const
{
	const AActor* WeaponActor = GetSpawnedActor(bFirstPerson);
	if (!WeaponActor)
	{
		return nullptr;
	}

	// The receiver is the root. Searching by class would just as happily return the magazine,
	// which is also a skeletal mesh and is very much not the thing montages play on.
	if (USkeletalMeshComponent* RootMesh = Cast<USkeletalMeshComponent>(WeaponActor->GetRootComponent()))
	{
		return RootMesh;
	}

	return WeaponActor->FindComponentByClass<USkeletalMeshComponent>();
}

URSWeaponAssemblyComponent* URSWeaponInstance::GetWeaponAssembly(bool bFirstPerson) const
{
	const ARSWeaponActor* WeaponActor = Cast<ARSWeaponActor>(GetSpawnedActor(bFirstPerson));
	return WeaponActor ? WeaponActor->GetAssembly() : nullptr;
}

void URSWeaponInstance::ApplyAnimLayers()
{
	ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	if (!OwningPlayer)
	{
		return;
	}

	// Both halves on every machine: the viewmodel is drawn wherever someone looks through this
	// pawn's eyes, which a dead teammate can do from any machine.
	if (FirstPersonAnimLayer)
	{
		if (USkeletalMeshComponent* ViewmodelMesh = OwningPlayer->GetFirstPersonMesh())
		{
			ViewmodelMesh->LinkAnimClassLayers(FirstPersonAnimLayer);
		}
	}

	if (ThirdPersonAnimLayer)
	{
		if (USkeletalMeshComponent* BodyMesh = OwningPlayer->GetMesh())
		{
			BodyMesh->LinkAnimClassLayers(ThirdPersonAnimLayer);
		}
	}

	// The weapon meshes run their own graph on their own skeleton; that is what moves the weapon
	// in step with the arms without it being attached to a hand.
	if (WeaponAnimClass)
	{
		for (bool bFirstPerson : { true, false })
		{
			if (USkeletalMeshComponent* WeaponMesh = GetWeaponMeshComponent(bFirstPerson))
			{
				WeaponMesh->SetAnimInstanceClass(WeaponAnimClass);
			}
		}
	}
}

void URSWeaponInstance::ClearAnimLayers()
{
	ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	if (!OwningPlayer)
	{
		return;
	}

	if (FirstPersonAnimLayer)
	{
		if (USkeletalMeshComponent* ViewmodelMesh = OwningPlayer->GetFirstPersonMesh())
		{
			ViewmodelMesh->UnlinkAnimClassLayers(FirstPersonAnimLayer);
		}
	}

	if (ThirdPersonAnimLayer)
	{
		if (USkeletalMeshComponent* BodyMesh = OwningPlayer->GetMesh())
		{
			BodyMesh->UnlinkAnimClassLayers(ThirdPersonAnimLayer);
		}
	}
}

void URSWeaponInstance::UpdateFiringTime()
{
	if (const UWorld* World = GetWorld())
	{
		TimeLastFired = World->GetTimeSeconds();
	}
}

float URSWeaponInstance::GetTimeSinceLastInteractedWith() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const double WorldTime = World->GetTimeSeconds();
	double Result = WorldTime - TimeLastEquipped;

	if (TimeLastFired > 0.0)
	{
		Result = FMath::Min(Result, WorldTime - TimeLastFired);
	}

	return static_cast<float>(Result);
}

void URSWeaponInstance::PlayFireMontages(float PlayRate)
{
	PlayWeaponMontagePair(FirstPersonWeaponFireMontage, ThirdPersonWeaponFireMontage, PlayRate);
	PlayMontagePair(FirstPersonFireMontage, ThirdPersonFireMontage, PlayRate);
}

void URSWeaponInstance::PlayFireEffects(float PlayRate)
{
	PlayFireMontages(PlayRate);

	// One copy only, the one on screen here: the viewmodel for a view through the shooter's eyes —
	// theirs, or a dead teammate's watching them in first person — and the world copy for every
	// other. The hidden copy exists too; a flash on it would still light the scene, and its sound
	// would play a second time.
	const ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	const bool bFirstPersonHere = OwningPlayer && OwningPlayer->IsViewedInFirstPerson();
	ARSWeaponActor* VisibleWeapon = Cast<ARSWeaponActor>(GetSpawnedActor(bFirstPersonHere));

	// The kick is the viewmodel's, so it is only given where the viewmodel is what is seen.
	if (bFirstPersonHere)
	{
		if (URSFirstPersonRecoilComponent* ViewmodelRecoil = OwningPlayer->FindComponentByClass<URSFirstPersonRecoilComponent>())
		{
			ViewmodelRecoil->AddRecoil(Recoil);
		}
	}

	if (VisibleWeapon)
	{
		VisibleWeapon->SpawnMuzzleEffect(MuzzleFlash);
		VisibleWeapon->SpawnMuzzleEffect(MuzzleSmoke, /*bFollowMuzzle=*/ false);

		for (USoundBase* Sound : FireSounds)
		{
			VisibleWeapon->PlayMuzzleSound(Sound);
		}
	}
}

void URSWeaponInstance::PlayAimInSound()
{
	if (!AimInSound)
	{
		return;
	}

	// The same copy a shot's report comes from, for the same reason: heard once, from where it is seen.
	const ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	const bool bFirstPersonHere = OwningPlayer && OwningPlayer->IsViewedInFirstPerson();
	if (const AActor* VisibleWeapon = GetSpawnedActor(bFirstPersonHere))
	{
		UGameplayStatics::SpawnSoundAttached(AimInSound, VisibleWeapon->GetRootComponent());
	}
}

void URSWeaponInstance::PlayReloadMontages(float PlayRate)
{
	// Before the montages: their visibility windows may show the spare on the very first frame.
	ShowSpareMagazineForReload();

	PlayWeaponMontagePair(FirstPersonWeaponReloadMontage, ThirdPersonWeaponReloadMontage, PlayRate);
	PlayMontagePair(FirstPersonReloadMontage, ThirdPersonReloadMontage, PlayRate);
}

void URSWeaponInstance::StopReloadMontages()
{
	StopWeaponMontagePair(FirstPersonWeaponReloadMontage, ThirdPersonWeaponReloadMontage);
	StopMontagePair(FirstPersonReloadMontage, ThirdPersonReloadMontage);
}

void URSWeaponInstance::PlayHolsterMontages(float PlayRate)
{
	PlayWeaponMontagePair(FirstPersonWeaponHolsterMontage, ThirdPersonWeaponHolsterMontage, PlayRate);
	PlayMontagePair(FirstPersonHolsterMontage, ThirdPersonHolsterMontage, PlayRate);
}

void URSWeaponInstance::PlayMeleeStrike(int32 Variant)
{
	const FRSMeleeStrike* Strike = MeleeStrikes ? MeleeStrikes->GetStrike(Variant) : nullptr;
	UWorld* World = GetWorld();
	if (!Strike || !World)
	{
		return;
	}

	const float PlayRate = MeleeStrikes->PlayRate;
	PlayMontagePair(Strike->FirstPersonMontage, Strike->ThirdPersonMontage, PlayRate);

	// Data rather than a notify in the montages, for the reason a shot's report is: the body's montage
	// plays on the striker's machine too, hidden, and a notify in both halves would be heard twice there.
	if (MeleeStrikes->WhooshSound)
	{
		World->GetTimerManager().SetTimer(MeleeWhooshTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
			const bool bFirstPersonHere = OwningPlayer && OwningPlayer->IsViewedInFirstPerson();
			if (const AActor* VisibleWeapon = GetSpawnedActor(bFirstPersonHere); VisibleWeapon && MeleeStrikes)
			{
				UGameplayStatics::SpawnSoundAttached(MeleeStrikes->WhooshSound, VisibleWeapon->GetRootComponent());
			}
		}), FMath::Max(Strike->WhooshTime / PlayRate, KINDA_SMALL_NUMBER), /*bLoop=*/ false);
	}
}

void URSWeaponInstance::StopMeleeStrike()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MeleeWhooshTimerHandle);
	}

	if (!MeleeStrikes)
	{
		return;
	}

	// Every montage in the set, not only the one this strike started: the cue's removal carries no
	// word of which it was, and each pair stops only where it is still playing.
	TArray<UAnimMontage*> FirstPerson;
	TArray<UAnimMontage*> ThirdPerson;
	MeleeStrikes->GetAllMontages(FirstPerson, ThirdPerson);
	for (int32 Index = 0; Index < FMath::Max(FirstPerson.Num(), ThirdPerson.Num()); ++Index)
	{
		StopMontagePair(FirstPerson.IsValidIndex(Index) ? FirstPerson[Index] : nullptr, ThirdPerson.IsValidIndex(Index) ? ThirdPerson[Index] : nullptr);
	}
}

void URSWeaponInstance::PlayMeleeImpact(const FVector& Location, const FVector& Normal) const
{
	if (!MeleeStrikes)
	{
		return;
	}

	// In the world, where the blow landed: seen and heard the same by everybody.
	if (MeleeStrikes->ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, MeleeStrikes->ImpactSound, Location);
	}
	if (MeleeStrikes->ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MeleeStrikes->ImpactEffect, Location, Normal.Rotation());
	}
}

float URSWeaponInstance::GetEquipDuration() const
{
	return GetPairLength(FirstPersonEquipMontage, ThirdPersonEquipMontage);
}

float URSWeaponInstance::GetHolsterDuration() const
{
	return GetPairLength(FirstPersonHolsterMontage, ThirdPersonHolsterMontage);
}

float URSWeaponInstance::GetReloadDuration() const
{
	return GetPairLength(FirstPersonReloadMontage, ThirdPersonReloadMontage);
}

float URSWeaponInstance::GetPairLength(const UAnimMontage* FirstPersonMontage, const UAnimMontage* ThirdPersonMontage)
{
	// Length comes from the asset rather than from whether a montage actually played, so a
	// dedicated server with no meshes to animate still measures the same duration the clients see.
	// GetPlayLength ignores the montage's own RateScale, which Montage_Play does apply. The reload
	// montages ship at 1.1, so measuring the raw length left the ability running a third of a
	// second past the animation — the weapon snapped back to its idle pose with the reload parts
	// still swapped, and the magazine vanished until EndAbility caught up.
	const auto GetOnScreenLength = [](const UAnimMontage* Montage)
	{
		return (Montage->RateScale > 0.0f) ? (Montage->GetPlayLength() / Montage->RateScale) : Montage->GetPlayLength();
	};

	float LongestLength = 0.0f;

	if (FirstPersonMontage)
	{
		LongestLength = FMath::Max(LongestLength, GetOnScreenLength(FirstPersonMontage));
	}

	if (ThirdPersonMontage)
	{
		LongestLength = FMath::Max(LongestLength, GetOnScreenLength(ThirdPersonMontage));
	}

	return LongestLength;
}

void URSWeaponInstance::PlayWeaponMontagePair(UAnimMontage* FirstPersonMontage, UAnimMontage* ThirdPersonMontage, float PlayRate)
{
	if (PlayRate <= 0.0f)
	{
		PlayRate = 1.0f;
	}

	// The character half decides the timing; this half only has to start alongside it.
	if (FirstPersonMontage)
	{
		if (USkeletalMeshComponent* WeaponMesh = GetWeaponMeshComponent(/*bFirstPerson=*/ true))
		{
			if (UAnimInstance* WeaponAnim = WeaponMesh->GetAnimInstance())
			{
				WeaponAnim->Montage_Play(FirstPersonMontage, PlayRate);
			}
		}
	}

	if (ThirdPersonMontage)
	{
		if (USkeletalMeshComponent* WeaponMesh = GetWeaponMeshComponent(/*bFirstPerson=*/ false))
		{
			if (UAnimInstance* WeaponAnim = WeaponMesh->GetAnimInstance())
			{
				WeaponAnim->Montage_Play(ThirdPersonMontage, PlayRate);
			}
		}
	}
}

void URSWeaponInstance::StopWeaponMontagePair(const UAnimMontage* FirstPersonMontage, const UAnimMontage* ThirdPersonMontage)
{
	// Named montage by montage, so whatever has already replaced it in the slot — a shot, a holster —
	// is left alone.
	const auto StopOn = [](USkeletalMeshComponent* WeaponMesh, const UAnimMontage* Montage)
	{
		UAnimInstance* WeaponAnim = (WeaponMesh && Montage) ? WeaponMesh->GetAnimInstance() : nullptr;
		if (WeaponAnim && WeaponAnim->Montage_IsPlaying(Montage))
		{
			WeaponAnim->Montage_Stop(Montage->BlendOut.GetBlendTime(), Montage);
		}
	};

	StopOn(GetWeaponMeshComponent(/*bFirstPerson=*/ true), FirstPersonMontage);
	StopOn(GetWeaponMeshComponent(/*bFirstPerson=*/ false), ThirdPersonMontage);
}

void URSWeaponInstance::StopMontagePair(const UAnimMontage* FirstPersonMontage, const UAnimMontage* ThirdPersonMontage)
{
	ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	if (!OwningPlayer)
	{
		return;
	}

	const auto StopOn = [](const USkeletalMeshComponent* Mesh, const UAnimMontage* Montage)
	{
		UAnimInstance* AnimInstance = (Mesh && Montage) ? Mesh->GetAnimInstance() : nullptr;
		if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
		{
			AnimInstance->Montage_Stop(Montage->BlendOut.GetBlendTime(), Montage);
		}
	};

	StopOn(OwningPlayer->GetFirstPersonMesh(), FirstPersonMontage);
	StopOn(OwningPlayer->GetMesh(), ThirdPersonMontage);
}

void URSWeaponInstance::PlayMontagePair(UAnimMontage* FirstPersonMontage, UAnimMontage* ThirdPersonMontage, float PlayRate)
{
	if (PlayRate <= 0.0f)
	{
		PlayRate = 1.0f;
	}

	ARSPlayerCharacter* OwningPlayer = Cast<ARSPlayerCharacter>(GetPawn());
	if (!OwningPlayer)
	{
		return;
	}

	// The viewmodel exists only where the pawn is controlled from, so this is a no-op everywhere
	// else rather than a branch on net role.
	if (FirstPersonMontage)
	{
		if (const USkeletalMeshComponent* ViewmodelMesh = OwningPlayer->GetFirstPersonMesh())
		{
			if (UAnimInstance* ViewmodelAnim = ViewmodelMesh->GetAnimInstance())
			{
				ViewmodelAnim->Montage_Play(FirstPersonMontage, PlayRate);
			}
		}
	}

	if (ThirdPersonMontage)
	{
		if (const USkeletalMeshComponent* BodyMesh = OwningPlayer->GetMesh())
		{
			if (UAnimInstance* BodyAnim = BodyMesh->GetAnimInstance())
			{
				BodyAnim->Montage_Play(ThirdPersonMontage, PlayRate);
			}
		}
	}
}
