// Round Survival

#include "Weapons/RSGameplayAbility_RangedWeapon.h"

#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Equipment/RSEquipmentInstance.h"
#include "TimerManager.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "Weapons/RSRangedWeaponInstance.h"
#include "Weapons/RSWeaponItemInstance.h"
#include "Weapons/RSWeaponStateComponent.h"
#include "Weapons/RSWeaponTargetData.h"

namespace RSConsoleVariables
{
	static float DrawShotTracesDuration = 0.0f;
	static FAutoConsoleVariableRef CVarDrawShotTraces(
		TEXT("RS.Weapon.DrawShotTraces"),
		DrawShotTracesDuration,
		TEXT("Seconds to draw the three segments of each shot: white aim, yellow muzzle resolve, red bullet. 0 disables."),
		ECVF_Default);
}

namespace
{
	/**
	 * A random direction inside a cone, biased towards the axis by Exponent.
	 *
	 * Exponent 1 spreads shots evenly across the cone; higher values pull them towards the centre,
	 * so the cone angle describes the worst case rather than the typical one.
	 */
	FVector RandomConeDirection(const FVector& Axis, float ConeHalfAngleRadians, float Exponent)
	{
		if (ConeHalfAngleRadians <= 0.0f)
		{
			return Axis.GetSafeNormal();
		}

		const float ConeHalfAngleDegrees = FMath::RadiansToDegrees(ConeHalfAngleRadians);
		const float BiasedAngle = ConeHalfAngleDegrees * FMath::Pow(FMath::FRand(), Exponent);
		const float AngleAround = FMath::FRand() * 360.0f;

		const FVector NormalisedAxis = Axis.GetSafeNormal();
		const FVector Perpendicular = NormalisedAxis.RotateAngleAxis(BiasedAngle, FVector::CrossProduct(NormalisedAxis, FVector::UpVector).GetSafeNormal());

		return Perpendicular.RotateAngleAxis(AngleAround, NormalisedAxis);
	}

	/**
	 * The fire cue's parameters: where the first bullet of the cartridge landed, for the impact.
	 * Left empty — a zero normal — when it hit nothing. A weapon with several bullets a cartridge
	 * shows the first one's impact only; one that needs them all would carry its hits in the cue's
	 * effect context instead.
	 *
	 * The surface is the one the shooter claimed unless the server replaced the hit with its own.
	 * Validation drops a hit zone the server did not touch itself, which is right for the damage
	 * and wrong for the blood: everybody saw the bullet land on the enemy.
	 */
	FGameplayCueParameters MakeFireCueParameters(const FGameplayAbilityTargetDataHandle& TargetData,
		const TWeakObjectPtr<UPhysicalMaterial>& ClaimedPhysicalMaterial)
	{
		FGameplayCueParameters Parameters;

		const FGameplayAbilityTargetData_SingleTargetHit* FirstHit = (TargetData.Num() > 0)
			? static_cast<const FGameplayAbilityTargetData_SingleTargetHit*>(TargetData.Get(0))
			: nullptr;

		if (FirstHit && FirstHit->HitResult.bBlockingHit)
		{
			Parameters.Location = FirstHit->HitResult.ImpactPoint;
			Parameters.Normal = FirstHit->HitResult.ImpactNormal;
			Parameters.PhysicalMaterial = FirstHit->bHitReplaced ? FirstHit->HitResult.PhysMaterial : ClaimedPhysicalMaterial;
		}

		return Parameters;
	}
}

URSGameplayAbility_RangedWeapon::URSGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Fires on the press edge, predicted locally and confirmed by the server.
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;

	// Pulling the trigger mid-reload shoots. The reload is cancelled by tag, so this ability never
	// has to know that reloading exists — anything else tagged the same way is cancelled too. Only a
	// pull does it: a reload ends a burst in progress (it cancels this ability), and a trigger held
	// through it does not start firing again until the reload is over (ShouldActivateWhileInputHeld).
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Reload);

	// A weapon on its way in or out of the player's hands does not fire: mid-switch, or coming back up
	// after its holder went down.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_SwitchingWeapon);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_GoingDown);

	// Nor does one whose holder's hands are taken by a revive or a station. Most interactions leave
	// the gun working: only those that say they take the hands put this tag on.
	// Checked before every shot as well, so such an interaction started mid-burst ends the burst.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_HandsBusy);

	// Pulling the trigger ends a sprint, and the shot's state keeps a sprint still held from coming
	// back until the burst is over.
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Sprint);
	ActivationOwnedTags.AddTag(RSGameplayTags::State_Firing);

	// What the reload cancels by: reloading stops a burst.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Fire);
	SetAssetTags(AssetTags);
}

namespace
{
	URSWeaponStateComponent* FindWeaponStateComponent(const AActor* AvatarActor)
	{
		const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
		AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;

		return Controller ? Controller->FindComponentByClass<URSWeaponStateComponent>() : nullptr;
	}
}

URSRangedWeaponInstance* URSGameplayAbility_RangedWeapon::GetWeaponInstance() const
{
	return Cast<URSRangedWeaponInstance>(GetAssociatedEquipment());
}

bool URSGameplayAbility_RangedWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Granted by something that is not a ranged weapon: fail rather than trace from nothing.
	const URSRangedWeaponInstance* Weapon = Cast<URSRangedWeaponInstance>(FindEquipmentFromSpec(Handle, ActorInfo));
	if (!Weapon)
	{
		UE_LOG(LogRSAbility, Error, TEXT("[%s] activated with no ranged weapon instance as its source object."), *GetNameSafe(this));
		return false;
	}

	// No cadence check here: the cadence is per shot, not per pull, and the server checks each shot
	// against its budget as it arrives (OnTargetDataReadyCallback).
	return true;
}

bool URSGameplayAbility_RangedWeapon::ShouldActivateWhileInputHeld(const FGameplayAbilitySpec& Spec, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const URSRangedWeaponInstance* Weapon = Cast<URSRangedWeaponInstance>(FindEquipmentFromSpec(Spec.Handle, ActorInfo));
	if (!Weapon || !Weapon->IsAutomatic())
	{
		return false;
	}

	// A reload stopped the burst; a trigger still held from before it does not cancel it. Only a
	// fresh pull does, through the press path, which this does not govern. Once the reload is over,
	// a held trigger fires again.
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return !ASC || !ASC->HasMatchingGameplayTag(RSGameplayTags::State_Reloading);
}

void URSGameplayAbility_RangedWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	// Bound before anything else: on the server this delegate is how the client's shot arrives,
	// and on the shooter it is how the local trace comes back.
	TargetDataReadyDelegateHandle = ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		.AddUObject(this, &ThisClass::OnTargetDataReadyCallback);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Only the machine that owns the aim fires. The server's copy stays active for the length of
	// the pull and takes each shot as its target data arrives; the shooter's end ends it.
	if (ActorInfo->IsLocallyControlled())
	{
		bTriggerHeld = true;
		bHasFiredThisPull = false;

		// A pull that comes before the cadence allows waits for it rather than firing early. One
		// after a pause starts now: idle time is never banked as shots.
		URSRangedWeaponInstance* Weapon = GetWeaponInstance();
		const UWorld* World = GetWorld();
		if (Weapon && World)
		{
			Weapon->SetNextShotTime(FMath::Max(Weapon->GetNextShotTime(), World->GetTimeSeconds()));
		}

		FireDueShots();
	}
}

void URSGameplayAbility_RangedWeapon::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	bTriggerHeld = false;

	// Between shots of a held trigger, letting go ends the pull. A pull still waiting for its first
	// shot — pulled again before the cadence allowed — keeps waiting, fires it, and ends then: a tap
	// is always a shot.
	if (bHasFiredThisPull && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
	}
}

void URSGameplayAbility_RangedWeapon::FireDueShots()
{
	ShotTimerHandle.Invalidate();

	URSRangedWeaponInstance* Weapon = GetWeaponInstance();
	UWorld* World = GetWorld();
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!Weapon || !World || !ASC || !IsActive())
	{
		return;
	}

	// Every shot that has come due by now, which is more than one when a frame is longer than the
	// weapon's interval. Each one advances the clock by exactly one interval from when it was due.
	while (Weapon->GetNextShotTime() <= World->GetTimeSeconds())
	{
		// Whatever would have stopped the pull from starting stops it mid-burst too: a weapon switch
		// begun while the trigger is held, most of all. Before each shot, since a pull now spans many.
		if (ASC->HasAnyMatchingGameplayTags(ActivationBlockedTags))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
			return;
		}

		Weapon->SetNextShotTime(Weapon->GetNextShotTime() + Weapon->GetTimeBetweenShots());
		bHasFiredThisPull = true;

		StartRangedWeaponTargeting();

		// The shot can end the pull itself — an empty magazine fails its commit.
		if (!IsActive())
		{
			return;
		}

		// One shot per pull for a semi-automatic weapon, and the last one for an automatic weapon
		// whose trigger has come up.
		if (!Weapon->IsAutomatic() || !bTriggerHeld)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
			return;
		}
	}

	// The timer only wakes this up; which shots are due is decided above, by world time. Waking a
	// frame early finds nothing due and sets it again; waking late fires the late shot at once.
	const float UntilNextShot = static_cast<float>(Weapon->GetNextShotTime() - World->GetTimeSeconds());
	World->GetTimerManager().SetTimer(ShotTimerHandle, this, &ThisClass::FireDueShots, FMath::Max(UntilNextShot, KINDA_SMALL_NUMBER), /*bLoop=*/ false);
}

void URSGameplayAbility_RangedWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (ScopeLockCount > 0)
		{
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo,
				bReplicateEndAbility, bWasCancelled));
			return;
		}

		if (UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get())
		{
			// Both halves matter: leaving the delegate bound would fire this shot's callback again
			// on the next activation, and leaving the data would keep it queued on the server.
			ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
				.Remove(TargetDataReadyDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}

		// Whatever ended the pull — release, empty magazine, a switch, the weapon going away — no
		// shot is left waiting to wake a finished ability.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ShotTimerHandle);
		}
		bTriggerHeld = false;

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

bool URSGameplayAbility_RangedWeapon::BuildFiringSolution(OUT FRSFiringSolution& OutSolution) const
{
	URSRangedWeaponInstance* Weapon = GetWeaponInstance();
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	UWorld* World = GetWorld();

	if (!Weapon || !AvatarPawn || !Controller || !World)
	{
		return false;
	}

	OutSolution.Weapon = Weapon;

	// The view point, not the mesh. On the server this is the replicated control rotation, which
	// is the same input the client used, which is what makes the two traces comparable.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		ViewLocation = AvatarPawn->GetPawnViewLocation();
		ViewRotation = Controller->GetControlRotation();
	}

	OutSolution.AimDirection = ViewRotation.Vector().GetSafeNormal();

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RSWeaponAim), /*bTraceComplex=*/ true, /*IgnoreActor=*/ AvatarPawn);
	TraceParams.bReturnPhysicalMaterial = true;
	AddAdditionalTraceIgnoreActors(TraceParams);

	// -- Step 1: aim segment. Where is the reticle pointing? ------------------------------------
	const FVector AimEnd = ViewLocation + (OutSolution.AimDirection * Weapon->GetMaxDamageRange());

	FHitResult AimHit;
	World->LineTraceSingleByChannel(AimHit, ViewLocation, AimEnd, ECC_Visibility, TraceParams);
	OutSolution.AimPoint = AimHit.bBlockingHit ? AimHit.ImpactPoint : AimEnd;

	// -- Step 2: muzzle resolve. Can the barrel actually be there? -------------------------------
	const FVector DesiredMuzzle = ViewLocation + ViewRotation.RotateVector(MuzzleOffsetFromCamera);

	FHitResult MuzzleHit;
	World->LineTraceSingleByChannel(MuzzleHit, ViewLocation, DesiredMuzzle, ECC_Visibility, TraceParams);

	if (MuzzleHit.bBlockingHit)
	{
		// Pressed against a wall: the shot starts at the wall rather than inside or beyond it.
		// Firing is never blocked — the player just cannot shoot past what they are touching.
		const FVector TowardsMuzzle = (DesiredMuzzle - ViewLocation).GetSafeNormal();
		OutSolution.MuzzleOrigin = MuzzleHit.ImpactPoint - (TowardsMuzzle * MuzzlePullbackPadding);
	}
	else
	{
		OutSolution.MuzzleOrigin = DesiredMuzzle;
	}

	// The aim point can land behind the muzzle when a target is pressed against the player. Tracing
	// backwards from the muzzle would produce a shot that travels the wrong way, so the bullet
	// segment falls back to the aim segment for this shot only.
	const FVector MuzzleToAim = OutSolution.AimPoint - OutSolution.MuzzleOrigin;
	if ((MuzzleToAim | OutSolution.AimDirection) <= 0.0f)
	{
		OutSolution.bCollapsedToAimSegment = true;
		OutSolution.MuzzleOrigin = ViewLocation;
	}

#if ENABLE_DRAW_DEBUG
	if (RSConsoleVariables::DrawShotTracesDuration > 0.0f)
	{
		const float Duration = RSConsoleVariables::DrawShotTracesDuration;
		DrawDebugLine(World, ViewLocation, OutSolution.AimPoint, FColor::White, false, Duration, 0, 1.0f);
		DrawDebugLine(World, ViewLocation, OutSolution.MuzzleOrigin, FColor::Yellow, false, Duration, 0, 2.0f);
		DrawDebugSphere(World, OutSolution.AimPoint, 8.0f, 12, FColor::Green, false, Duration);
	}
#endif

	return true;
}

FHitResult URSGameplayAbility_RangedWeapon::WeaponTrace(const FVector& Start, const FVector& End, float SweepRadius,
	OUT TArray<FHitResult>& OutHits) const
{
	UWorld* World = GetWorld();
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RSWeaponBullet), /*bTraceComplex=*/ true, /*IgnoreActor=*/ AvatarPawn);
	TraceParams.bReturnPhysicalMaterial = true;
	AddAdditionalTraceIgnoreActors(TraceParams);

	TArray<FHitResult> Hits;

	if (SweepRadius > 0.0f)
	{
		World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(SweepRadius), TraceParams);
	}
	else
	{
		World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, TraceParams);
	}

	FHitResult Result(ForceInit);
	Result.TraceStart = Start;
	Result.TraceEnd = End;

	// One entry per actor: a sweep can report the same enemy several times and each entry would
	// later become its own damage application.
	for (const FHitResult& Hit : Hits)
	{
		const bool bAlreadyPresent = OutHits.ContainsByPredicate(
			[&Hit](const FHitResult& Other) { return Other.HitObjectHandle == Hit.HitObjectHandle; });

		if (!bAlreadyPresent)
		{
			OutHits.Add(Hit);
		}
	}

	if (OutHits.Num() > 0)
	{
		Result = OutHits.Last();
	}

	return Result;
}

void URSGameplayAbility_RangedWeapon::TraceBulletsInCartridge(const FRSFiringSolution& Solution, OUT TArray<FHitResult>& OutHits) const
{
	URSRangedWeaponInstance* Weapon = Solution.Weapon;
	if (!Weapon)
	{
		return;
	}

	const int32 BulletsPerCartridge = Weapon->GetBulletsPerCartridge();
	const float SpreadAngle = Weapon->GetCalculatedSpreadAngle() * Weapon->GetCalculatedSpreadAngleMultiplier();
	const float HalfSpreadRadians = FMath::DegreesToRadians(SpreadAngle * 0.5f);

	// -- Step 3: the shot itself, from the muzzle towards where the reticle landed ---------------
	const FVector MuzzleToAim = (Solution.AimPoint - Solution.MuzzleOrigin).GetSafeNormal();

	for (int32 BulletIndex = 0; BulletIndex < BulletsPerCartridge; ++BulletIndex)
	{
		// Spread is applied here, at the muzzle, not at the eye: a cone opened at the camera would
		// widen by the distance between camera and barrel before the bullet had gone anywhere.
		const FVector BulletDirection = RandomConeDirection(MuzzleToAim, HalfSpreadRadians, Weapon->GetSpreadExponent());
		const FVector BulletEnd = Solution.MuzzleOrigin + (BulletDirection * Weapon->GetMaxDamageRange());

		TArray<FHitResult> BulletHits;
		FHitResult Impact = WeaponTrace(Solution.MuzzleOrigin, BulletEnd, Weapon->GetBulletTraceSweepRadius(), BulletHits);

		if (!Impact.bBlockingHit)
		{
			// Still recorded: cues and tracers need a direction even when nothing was hit.
			Impact.Location = BulletEnd;
			Impact.ImpactPoint = BulletEnd;
		}

#if ENABLE_DRAW_DEBUG
		if (RSConsoleVariables::DrawShotTracesDuration > 0.0f)
		{
			const float Duration = RSConsoleVariables::DrawShotTracesDuration;
			DrawDebugLine(GetWorld(), Solution.MuzzleOrigin, Impact.ImpactPoint, FColor::Red, false, Duration, 0, 2.0f);
			DrawDebugSphere(GetWorld(), Impact.ImpactPoint, 6.0f, 12, FColor::Red, false, Duration);
		}
#endif

		UE_LOG(LogRSAbility, Verbose, TEXT("Shot %d: muzzle %s -> aim %s, hit [%s] at %s%s"),
			BulletIndex,
			*Solution.MuzzleOrigin.ToCompactString(),
			*Solution.AimPoint.ToCompactString(),
			*GetNameSafe(Impact.GetActor()),
			*Impact.ImpactPoint.ToCompactString(),
			Solution.bCollapsedToAimSegment ? TEXT(" (collapsed to aim segment)") : TEXT(""));

		OutHits.Add(Impact);
	}
}

void URSGameplayAbility_RangedWeapon::AddAdditionalTraceIgnoreActors(FCollisionQueryParams& TraceParams) const
{
	// The weapon actors are attached to the pawn and sit right where the shot begins.
	if (const URSEquipmentInstance* Equipment = GetAssociatedEquipment())
	{
		TraceParams.AddIgnoredActors(Equipment->GetSpawnedActors());
	}
}

void URSGameplayAbility_RangedWeapon::StartRangedWeaponTargeting()
{
	check(CurrentActorInfo);

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	// Before the trace, so spread recovery counts from this shot.
	if (URSRangedWeaponInstance* Weapon = GetWeaponInstance())
	{
		Weapon->UpdateFiringTime();
	}

	FRSFiringSolution Solution;
	if (!BuildFiringSolution(Solution))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	TArray<FHitResult> Hits;
	TraceBulletsInCartridge(Solution, Hits);

	FGameplayAbilityTargetDataHandle TargetData;
	TargetData.UniqueId = FMath::Rand();

	for (const FHitResult& Hit : Hits)
	{
		FRSGameplayAbilityTargetData_SingleTargetHit* NewTargetData = new FRSGameplayAbilityTargetData_SingleTargetHit();
		NewTargetData->HitResult = Hit;
		NewTargetData->CartridgeID = TargetData.UniqueId;
		TargetData.Add(NewTargetData);
	}

	// Recorded before the server has seen anything: the player gets feedback on this frame and the
	// server's verdict arrives later to keep or drop it.
	if (URSWeaponStateComponent* WeaponState = FindWeaponStateComponent(GetAvatarActorFromActorInfo()))
	{
		WeaponState->AddUnconfirmedHitMarkers(TargetData, Hits);
	}

	OnTargetDataReadyCallback(TargetData, FGameplayTag());
}

bool URSGameplayAbility_RangedWeapon::ValidateTargetData(OUT FGameplayAbilityTargetDataHandle& TargetData, OUT TOptional<FVector>& OutServerMuzzleOrigin) const
{
	FRSFiringSolution ServerSolution;
	if (!BuildFiringSolution(ServerSolution))
	{
		return false;
	}

	OutServerMuzzleOrigin = ServerSolution.MuzzleOrigin;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	bool bAllAccepted = true;

	for (int32 Index = 0; Index < TargetData.Num(); ++Index)
	{
		FGameplayAbilityTargetData_SingleTargetHit* SingleHit =
			static_cast<FGameplayAbilityTargetData_SingleTargetHit*>(TargetData.Get(Index));

		if (!SingleHit)
		{
			continue;
		}

		const FVector ClaimedImpact = SingleHit->HitResult.ImpactPoint;

		// Out of range before anything else is worth checking.
		const float ClaimedDistance = FVector::Dist(ServerSolution.MuzzleOrigin, ClaimedImpact);
		if (ClaimedDistance > ServerSolution.Weapon->GetMaxDamageRange() + ServerValidationTolerance)
		{
			bAllAccepted = false;
			SingleHit->bHitReplaced = true;
			SingleHit->HitResult = FHitResult();
			continue;
		}

		// The decisive test: trace the server's own bullet segment towards the claimed impact. If
		// something blocks it meaningfully earlier — the corner the client shot through — the
		// claim is replaced by what the server actually sees.
		//
		// The trace runs slightly past the claimed impact. Ending exactly on a surface is not
		// guaranteed to register it, and the server has to touch the body itself to read its
		// physical material.
		const FVector ShotDirection = (ClaimedImpact - ServerSolution.MuzzleOrigin).GetSafeNormal();
		const FVector ServerTraceEnd = ClaimedImpact + (ShotDirection * HitVerificationTraceExtension);

		TArray<FHitResult> ServerHits;
		const FHitResult ServerImpact = WeaponTrace(ServerSolution.MuzzleOrigin, ServerTraceEnd,
			ServerSolution.Weapon->GetBulletTraceSweepRadius(), ServerHits);

		const AActor* ClaimedActor = SingleHit->HitResult.GetActor();

		if (!ServerImpact.bBlockingHit)
		{
			// The server's bullet reached the claimed point and found nothing: usually the target's
			// pose differs on the server under lag. The hit stands, but the hit zone is the client's
			// word alone, so it is dropped and the shot counts at x1. No one gets a headshot the
			// server did not see.
			UE_LOG(LogRSAbility, Verbose, TEXT("Server did not touch [%s] at %s: hit kept, zone dropped."),
				*GetNameSafe(ClaimedActor), *ClaimedImpact.ToCompactString());

			SingleHit->HitResult.PhysMaterial = nullptr;
			continue;
		}

		const float ServerDistance = FVector::Dist(ServerSolution.MuzzleOrigin, ServerImpact.ImpactPoint);
		const bool bSameActor = ServerImpact.GetActor() == ClaimedActor;
		const bool bStoppedEarlier = ServerDistance < (ClaimedDistance - ServerValidationTolerance);

		// Something else, found only in the extension past the claimed point — a wall behind the
		// target — is not evidence against the claim. It is the same as finding nothing.
		const bool bOnlyPastClaim = !bSameActor && ServerDistance > ClaimedDistance;

		if (bStoppedEarlier || (!bSameActor && !bOnlyPastClaim))
		{
			UE_LOG(LogRSAbility, Verbose, TEXT("Rejected client hit on [%s] at %s; server's bullet stopped on [%s] at %s."),
				*GetNameSafe(ClaimedActor), *ClaimedImpact.ToCompactString(),
				*GetNameSafe(ServerImpact.GetActor()), *ServerImpact.ImpactPoint.ToCompactString());

			SingleHit->bHitReplaced = true;
			SingleHit->HitResult = ServerImpact;
			bAllAccepted = false;
		}
		else if (bSameActor)
		{
			// The server agrees on the target, so the hit zone is the one the server's own trace
			// touched. The client's material never reaches the damage execution.
			SingleHit->HitResult.PhysMaterial = ServerImpact.PhysMaterial;
		}
		else
		{
			UE_LOG(LogRSAbility, Verbose, TEXT("Server found only [%s] past the claimed hit on [%s] at %s: hit kept, zone dropped."),
				*GetNameSafe(ServerImpact.GetActor()), *GetNameSafe(ClaimedActor), *ClaimedImpact.ToCompactString());

			SingleHit->HitResult.PhysMaterial = nullptr;
		}
	}

	return bAllAccepted;
}

void URSGameplayAbility_RangedWeapon::ApplyDamageToTargets(const FGameplayAbilityTargetDataHandle& TargetData, const FVector& MuzzleOrigin)
{
	check(CurrentActorInfo);

	if (!CurrentActorInfo->IsNetAuthority() || TargetData.Num() == 0)
	{
		return;
	}

	if (!DamageEffect)
	{
		UE_LOG(LogRSAbility, Error, TEXT("[%s] has no DamageEffect; the shot hit but deals nothing."), *GetNameSafe(this));
		return;
	}

	const URSRangedWeaponInstance* Weapon = GetWeaponInstance();
	if (!Weapon)
	{
		return;
	}

	// The spec's context already carries this ability and, as its source object, the weapon
	// instance the ability was granted by. That is what the execution asks about falloff and zones.
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// The Weapon Forge's upgrade belongs to this copy of the weapon, so it scales this weapon's damage
	// and no other: the item says by how much.
	const URSWeaponItemInstance* Item = GetAssociatedItem();
	const float Damage = Weapon->GetBaseDamage() * (Item ? Item->GetDamageMultiplier() : 1.0f);

	SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Damage, Damage);
	SpecHandle.Data->GetContext().AddOrigin(MuzzleOrigin);

	// Applied through the target data rather than actor by actor: each hit gets its own copy of the
	// context with its own hit result, which is where the execution reads the physical material and
	// impact point. Hits on actors without an ability system — walls — are skipped here.
	ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetData);
}

void URSGameplayAbility_RangedWeapon::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	if (!ASC->FindAbilitySpecFromHandle(CurrentSpecHandle))
	{
		return;
	}

	FScopedPredictionWindow ScopedPrediction(ASC);

	// Taken by value: anything called below could otherwise invalidate the data mid-use.
	FGameplayAbilityTargetDataHandle LocalTargetData(MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));

	const bool bShouldNotifyServer = CurrentActorInfo->IsLocallyControlled() && !CurrentActorInfo->IsNetAuthority();
	if (bShouldNotifyServer)
	{
		ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
			LocalTargetData, ApplicationTag, ASC->ScopedPredictionKey);
	}

	// Set by validation on the server; stays unset on a client, which never applies damage.
	TOptional<FVector> ServerMuzzleOrigin;

	// Before validation can change it: the impact shows the surface the shooter saw (MakeFireCueParameters).
	TWeakObjectPtr<UPhysicalMaterial> ClaimedPhysicalMaterial;
	if (const FGameplayAbilityTargetData_SingleTargetHit* FirstHit = (LocalTargetData.Num() > 0)
		? static_cast<const FGameplayAbilityTargetData_SingleTargetHit*>(LocalTargetData.Get(0)) : nullptr)
	{
		ClaimedPhysicalMaterial = FirstHit->HitResult.PhysMaterial;
	}

#if WITH_SERVER_CODE
	if (CurrentActorInfo->IsNetAuthority())
	{
		// The server's own check on the cadence, shot by shot. The shooter keeps the cadence, so an
		// honest client never meets this; one that fires faster than its weapon has the extra shots
		// refused here. Spent as the shot arrives, before anything else is looked at.
		URSRangedWeaponInstance* Weapon = GetWeaponInstance();
		const UWorld* World = GetWorld();
		if (Weapon && World)
		{
			if (!Weapon->HasServerShotBudget(World->GetTimeSeconds()))
			{
				RejectShot(LocalTargetData);
				return;
			}

			Weapon->ConsumeServerShot(World->GetTimeSeconds());
		}

		const bool bTargetDataValid = ValidateTargetData(LocalTargetData, ServerMuzzleOrigin);

		if (URSWeaponStateComponent* WeaponState = FindWeaponStateComponent(GetAvatarActorFromActorInfo()))
		{
			// Only the indices the server overruled travel back, not the whole result: the client
			// already has its own copy and only needs to know which parts of it were wrong.
			TArray<uint8> HitReplaces;

			for (int32 Index = 0; (Index < LocalTargetData.Num()) && (Index < 255); ++Index)
			{
				const FGameplayAbilityTargetData_SingleTargetHit* SingleHit =
					static_cast<const FGameplayAbilityTargetData_SingleTargetHit*>(LocalTargetData.Get(Index));

				if (SingleHit && SingleHit->bHitReplaced)
				{
					HitReplaces.Add(static_cast<uint8>(Index));
				}
			}

			WeaponState->ClientConfirmTargetData(static_cast<uint16>(LocalTargetData.UniqueId), bTargetDataValid, HitReplaces);
		}
	}
#endif

	// Commit last: costs are only charged for a shot that actually happened, and an empty magazine
	// found here means the shot does not count at all. Without this call ApplyCost never runs and
	// the weapon fires for free.
	if (CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		if (URSRangedWeaponInstance* Weapon = GetWeaponInstance())
		{
			Weapon->AddSpread();

			// Cosmetic and last: the shot has already been traced, validated and charged, so a
			// missing montage costs nothing but the animation. A cue rather than a direct play,
			// so everybody else sees the shot too; this runs inside the prediction window, so the
			// shooter plays it now and the server's multicast skips them. It carries the impact too,
			// so the shooter shows where its own trace landed and everybody else where the
			// server's validated one did.
			K2_ExecuteGameplayCueWithParams(RSGameplayTags::Cue_Weapon_Fire, MakeFireCueParameters(LocalTargetData, ClaimedPhysicalMaterial));
		}

		// After the commit, so a shot the magazine could not pay for deals nothing. The target data
		// is the validated copy: any hit the server overruled has already been replaced by its own.
		if (ServerMuzzleOrigin.IsSet())
		{
			ApplyDamageToTargets(LocalTargetData, ServerMuzzleOrigin.GetValue());
		}

		K2_OnRangedWeaponTargetDataReady(LocalTargetData);

		// One pull carries many shots under the same activation, so each shot's data is let go of
		// once handled. Left cached, the server would report the next shot as overriding it.
		if (CurrentActorInfo->IsNetAuthority())
		{
			ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
	}
}

void URSGameplayAbility_RangedWeapon::RejectShot(const FGameplayAbilityTargetDataHandle& TargetData)
{
	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] refused a shot over the weapon's cadence."), *GetNameSafe(this));

	// Not charged, not applied, no cue. The shooter already showed its own, and drops its markers.
	if (URSWeaponStateComponent* WeaponState = FindWeaponStateComponent(GetAvatarActorFromActorInfo()))
	{
		WeaponState->ClientConfirmTargetData(static_cast<uint16>(TargetData.UniqueId), /*bSuccess=*/ false, TArray<uint8>());
	}

	if (UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
}
