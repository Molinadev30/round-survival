// Round Survival

#include "AbilitySystem/Abilities/RSGameplayAbility_PlayerMelee.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Tasks/RSAbilityTask_Lunge.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/RSPlayerCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Equipment/RSEquipmentManagerComponent.h"
#include "Equipment/RSWeaponInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "TimerManager.h"
#include "Weapons/RSMeleeStrikeSet.h"

namespace RSConsoleVariables
{
	static float DrawMeleeRaysDuration = 0.0f;
	static FAutoConsoleVariableRef CVarDrawMeleeRays(
		TEXT("RS.Melee.DrawRays"),
		DrawMeleeRaysDuration,
		TEXT("Seconds to draw the rays of each player strike: green for the one that landed, red for the rest. 0 disables."),
		ECVF_Default);
}

TArray<TWeakObjectPtr<AActor>> FRSGameplayAbilityTargetData_MeleePlan::GetActors() const
{
	TArray<TWeakObjectPtr<AActor>> Actors;
	if (LungeTarget.IsValid())
	{
		Actors.Add(LungeTarget);
	}
	return Actors;
}

bool FRSGameplayAbilityTargetData_MeleePlan::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << Variant;
	Ar << LungeTarget;

	bOutSuccess = true;
	return true;
}

URSGameplayAbility_PlayerMelee::URSGameplayAbility_PlayerMelee(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;

	ActivationOwnedTags.AddTag(RSGameplayTags::State_Melee);

	// The blow takes the hands from whatever had them, and holds them until it is over. Held keys bring
	// the cut abilities back once it is: the sprint and the sights by themselves, an automatic weapon's
	// trigger by its own held-input rule.
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Fire);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Reload);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_ADS);
	CancelAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Sprint);

	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Fire);
	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Reload);
	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_ADS);
	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(RSGameplayTags::Ability_SwitchWeapon);

	// A weapon on its way in or out, hands taken by a revive or a station, or a player on the floor.
	ActivationBlockedTags.AddTag(RSGameplayTags::State_SwitchingWeapon);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_HandsBusy);
	ActivationBlockedTags.AddTag(RSGameplayTags::State_Downed);

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RSGameplayTags::Ability_Melee);
	SetAssetTags(AssetTags);

	// Straight ahead, then the four sides at half the fan, then its four corners.
	FanPattern = {
		FVector2D(0.0f, 0.0f),
		FVector2D(0.5f, 0.0f), FVector2D(-0.5f, 0.0f), FVector2D(0.0f, 0.5f), FVector2D(0.0f, -0.5f),
		FVector2D(-1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-1.0f, -1.0f), FVector2D(1.0f, -1.0f),
	};
}

void URSGameplayAbility_PlayerMelee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	// On the server this is how the striker's plan and blow arrive; on the striker, how its own come back.
	TargetDataReadyDelegateHandle = ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		.AddUObject(this, &ThisClass::OnTargetDataReady);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StrikeStartTime = GetWorld()->GetTimeSeconds();
	StrikeReach = Reach;
	StrikeTarget.Reset();

	// Until the plan says which strike this is, the server holds the hands for the longest one. The
	// plan arrives right behind the activation, so this is only ever a fallback.
	GetWorld()->GetTimerManager().SetTimer(FinishTimerHandle, this, &ThisClass::FinishStrike, GetLongestStrikeDuration(), /*bLoop=*/ false);

	if (!ActorInfo->IsLocallyControlled())
	{
		return;
	}

	// The striker decides the strike now: a lunge when an enemy is in lunge reach, the next of the
	// alternating strikes otherwise.
	AActor* LungeTarget = FindLungeTarget();
	const URSMeleeStrikeSet* StrikeSet = GetStrikeSet();
	int32 Variant = 0;
	if (LungeTarget)
	{
		Variant = INDEX_NONE;
	}
	else if (StrikeSet && StrikeSet->GetNumStrikes() > 0)
	{
		Variant = NextStrike % StrikeSet->GetNumStrikes();
		NextStrike = (NextStrike + 1) % StrikeSet->GetNumStrikes();
	}

	BeginStrike(Variant, LungeTarget);

	FRSGameplayAbilityTargetData_MeleePlan* Plan = new FRSGameplayAbilityTargetData_MeleePlan();
	Plan->Variant = Variant;
	Plan->LungeTarget = LungeTarget;

	FGameplayAbilityTargetDataHandle PlanData;
	PlanData.Add(Plan);
	OnTargetDataReady(PlanData, FGameplayTag());

	float TimeToImpact = 0.0f;
	float Duration = 0.0f;
	GetStrikeTiming(Variant, TimeToImpact, Duration);
	if (TimeToImpact > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ImpactTimerHandle, this, &ThisClass::Strike, TimeToImpact, /*bLoop=*/ false);
	}
	else
	{
		Strike();
	}
}

void URSGameplayAbility_PlayerMelee::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
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
			ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
				.Remove(TargetDataReadyDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}

		// Cut short — by going down, by a revive — the blow that had not landed yet never does.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ImpactTimerHandle);
			World->GetTimerManager().ClearTimer(FinishTimerHandle);
		}

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

const URSMeleeStrikeSet* URSGameplayAbility_PlayerMelee::GetStrikeSet() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const URSEquipmentManagerComponent* Equipment = Avatar ? Avatar->FindComponentByClass<URSEquipmentManagerComponent>() : nullptr;
	const URSWeaponInstance* Weapon = Equipment ? Equipment->GetFirstInstanceOfType<URSWeaponInstance>() : nullptr;
	return Weapon ? Weapon->GetMeleeStrikes() : nullptr;
}

void URSGameplayAbility_PlayerMelee::GetStrikeTiming(int32 Variant, float& OutImpactDelay, float& OutDuration) const
{
	const URSMeleeStrikeSet* StrikeSet = GetStrikeSet();
	const FRSMeleeStrike* Strike = StrikeSet ? StrikeSet->GetStrike(Variant) : nullptr;
	if (Strike)
	{
		OutImpactDelay = Strike->ImpactTime / StrikeSet->PlayRate;
		OutDuration = Strike->Duration / StrikeSet->PlayRate;
	}
	else
	{
		OutImpactDelay = ImpactDelay;
		OutDuration = StrikeDuration;
	}

	// The hands never come back before the blow has landed.
	OutDuration = FMath::Max(OutDuration, OutImpactDelay + KINDA_SMALL_NUMBER);
}

float URSGameplayAbility_PlayerMelee::GetLongestStrikeDuration() const
{
	float ImpactSeconds = 0.0f;
	float Longest = 0.0f;
	GetStrikeTiming(INDEX_NONE, ImpactSeconds, Longest);

	const URSMeleeStrikeSet* StrikeSet = GetStrikeSet();
	for (int32 Variant = 0; StrikeSet && Variant < StrikeSet->GetNumStrikes(); ++Variant)
	{
		float Duration = 0.0f;
		GetStrikeTiming(Variant, ImpactSeconds, Duration);
		Longest = FMath::Max(Longest, Duration);
	}
	return Longest;
}

void URSGameplayAbility_PlayerMelee::BeginStrike(int32 Variant, AActor* LungeTarget)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float TimeToImpact = 0.0f;
	float Duration = 0.0f;
	GetStrikeTiming(Variant, TimeToImpact, Duration);

	// Counted from when the strike began on this machine, which on the server is when the activation
	// arrived: the plan follows it within the frame.
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - StrikeStartTime);
	World->GetTimerManager().SetTimer(FinishTimerHandle, this, &ThisClass::FinishStrike, FMath::Max(Duration - Elapsed, KINDA_SMALL_NUMBER), /*bLoop=*/ false);

	if (LungeTarget)
	{
		StrikeReach = GetLungeReachNow();
		StrikeTarget = LungeTarget;
		StartLunge(LungeTarget, TimeToImpact - Elapsed);
	}
}

bool URSGameplayAbility_PlayerMelee::GetStrikeView(FVector& OutEye, FRotator& OutRotation) const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}

	// The same view point the shot is traced from, which the server reproduces from the replicated
	// control rotation.
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->GetPlayerViewPoint(OutEye, OutRotation);
	}
	else
	{
		OutEye = AvatarPawn->GetPawnViewLocation();
		OutRotation = Controller->GetControlRotation();
	}

	OutRotation.Roll = 0.0f;
	return true;
}

bool URSGameplayAbility_PlayerMelee::IsStrikeTarget(const AActor* Actor) const
{
	// Teammates stop a ray like a wall does: a blow at a teammate is no blow, and players cannot hurt
	// each other anyway.
	if (!Actor || Actor->IsA<ARSPlayerCharacter>())
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	return TargetASC && !TargetASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead);
}

float URSGameplayAbility_PlayerMelee::GetLungeReachNow() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	return Movement && Movement->IsFalling() ? LungeReachInAir : LungeReach;
}

float URSGameplayAbility_PlayerMelee::GetApproachSpeed(const AActor* Target) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Target)
	{
		return 0.0f;
	}

	const FVector ToTarget = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
	const FVector Velocity = Avatar->GetVelocity();
	return static_cast<float>(FVector::DotProduct(FVector(Velocity.X, Velocity.Y, 0.0f), ToTarget));
}

float URSGameplayAbility_PlayerMelee::GetAllowedDistance(float InReach) const
{
	// The fan's farthest corner, measured from the eye, plus slack. The client's word for where the
	// rays started is never used.
	return static_cast<float>(FVector(InReach, FanHalfExtent.X, FanHalfExtent.Y).Size()) + ServerValidationTolerance;
}

AActor* URSGameplayAbility_PlayerMelee::FindLungeTarget() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}

	// The same fan as the blow, cast as far as a lunge reaches from where the player is.
	FHitResult Hit;
	if (!FindStrikeTarget(GetLungeReachNow(), Hit))
	{
		return nullptr;
	}

	// Already as close as a lunge would leave them: the blow lands from here.
	AActor* Target = Hit.GetActor();
	if (FVector::DistSquared2D(Avatar->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(LungeStopDistance))
	{
		return nullptr;
	}

	// The lunge carries on a move the player is already making. Standing, backing off or passing the
	// enemy by, they strike from where they are.
	if (GetApproachSpeed(Target) < LungeMinApproachSpeed)
	{
		return nullptr;
	}

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] lunges at [%s], %.0f cm away"), *GetNameSafe(Avatar), *GetNameSafe(Target),
		FVector::Dist2D(Avatar->GetActorLocation(), Target->GetActorLocation()));
	return Target;
}

void URSGameplayAbility_PlayerMelee::StartLunge(AActor* Target, float TimeToImpact)
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || TimeToImpact <= 0.0f)
	{
		return;
	}

	// It ends with the player stopped where it put them, however it ends — out of time or arrived — so
	// the striker and the server, which may see it end a frame apart and for different reasons, leave
	// it at the same speed. What it carried would otherwise close the last few centimetres, and there
	// the two capsules touch and push the player round the enemy. A fall keeps falling.
	URSAbilityTask_Lunge* LungeTask = URSAbilityTask_Lunge::Lunge(this, Target, TimeToImpact, LungeStopDistance, LungeMaxSpeed, /*FinishMaxSpeed=*/ 0.0f);
	LungeTask->ReadyForActivation();
}

bool URSGameplayAbility_PlayerMelee::ValidateLungeTarget(const AActor* Target) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	FVector Eye;
	FRotator ViewRotation;
	if (!IsStrikeTarget(Target) || !Avatar || !World || !GetStrikeView(Eye, ViewRotation))
	{
		return false;
	}

	// Within a lunge's reach of the server's eye, to the nearest part of the enemy.
	FVector BoundsOrigin;
	FVector BoundsExtent;
	Target->GetActorBounds(/*bOnlyCollidingComponents=*/ true, BoundsOrigin, BoundsExtent);
	const float Distance = FMath::Sqrt(FBox(BoundsOrigin - BoundsExtent, BoundsOrigin + BoundsExtent).ComputeSquaredDistanceToPoint(Eye));
	const float MaxDistance = GetAllowedDistance(GetLungeReachNow());
	if (Distance > MaxDistance)
	{
		UE_LOG(LogRSAbility, Verbose, TEXT("Lunge at [%s] refused: %.0f cm from the server's eye, %.0f allowed."), *GetNameSafe(Target), Distance, MaxDistance);
		return false;
	}

	// Moving towards it on the server's copy too, with half the speed asked of the striker: the copy
	// is behind the striker's own by the moves still on their way.
	const float ApproachSpeed = GetApproachSpeed(Target);
	if (ApproachSpeed < 0.5f * LungeMinApproachSpeed)
	{
		UE_LOG(LogRSAbility, Verbose, TEXT("Lunge at [%s] refused: approaching at %.0f cm/s, %.0f needed."), *GetNameSafe(Target), ApproachSpeed, 0.5f * LungeMinApproachSpeed);
		return false;
	}

	// And not through a wall.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RSPlayerMeleeValidateLunge), /*bTraceComplex=*/ true, Avatar);
	Params.AddIgnoredActor(Target);
	FCollisionObjectQueryParams WorldOnly;
	WorldOnly.AddObjectTypesToQuery(ECC_WorldStatic);
	WorldOnly.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Blocker;
	if (World->LineTraceSingleByObjectType(Blocker, Eye, Target->GetActorLocation(), WorldOnly, Params))
	{
		UE_LOG(LogRSAbility, Verbose, TEXT("Lunge at [%s] refused: [%s] is in the way."), *GetNameSafe(Target), *GetNameSafe(Blocker.GetActor()));
		return false;
	}

	return true;
}

bool URSGameplayAbility_PlayerMelee::FindStrikeTarget(float InReach, FHitResult& OutHit) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	FVector Eye;
	FRotator ViewRotation;
	if (!Avatar || !World || !GetStrikeView(Eye, ViewRotation))
	{
		return false;
	}

	const FRotationMatrix ViewAxes(ViewRotation);
	const FVector Forward = ViewAxes.GetUnitAxis(EAxis::X);
	const FVector Right = ViewAxes.GetUnitAxis(EAxis::Y);
	const FVector Up = ViewAxes.GetUnitAxis(EAxis::Z);

	const FVector Start = Eye - Forward * OriginPullback;
	const FVector Ahead = Eye + Forward * InReach;

	// The player and whatever hangs on them — the weapon at their side, in their hands — are not in the way.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RSPlayerMelee), /*bTraceComplex=*/ true, Avatar);
	TArray<AActor*> Attached;
	Avatar->GetAttachedActors(Attached, /*bResetArray=*/ true, /*bRecursivelyIncludeAttachedActors=*/ true);
	Params.AddIgnoredActors(Attached);
	Params.bReturnPhysicalMaterial = true;

	bool bFound = false;
	for (const FVector2D& Offset : FanPattern)
	{
		const FVector End = Ahead + Right * (FanHalfExtent.X * Offset.X) + Up * (FanHalfExtent.Y * Offset.Y);

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		const bool bLanded = bHit && IsStrikeTarget(Hit.GetActor());

#if ENABLE_DRAW_DEBUG
		if (RSConsoleVariables::DrawMeleeRaysDuration > 0.0f)
		{
			DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, bLanded ? FColor::Green : FColor::Red, false,
				RSConsoleVariables::DrawMeleeRaysDuration, 0, 0.5f);
		}
#endif

		if (bLanded)
		{
			OutHit = Hit;
			bFound = true;
			break;
		}
	}

	return bFound;
}

bool URSGameplayAbility_PlayerMelee::FindStrikeOn(const AActor* Target, float InReach, FHitResult& OutHit) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	FVector Eye;
	FRotator ViewRotation;
	if (!IsStrikeTarget(Target) || !Avatar || !World || !GetStrikeView(Eye, ViewRotation))
	{
		return false;
	}

	// The middle of the body: straight across at eye height when the eye is level with it, its chest or
	// hips from above or below. Only the mesh blocks the ray, so the centre is where it is sure to land.
	FVector BoundsOrigin;
	FVector BoundsExtent;
	Target->GetActorBounds(/*bOnlyCollidingComponents=*/ true, BoundsOrigin, BoundsExtent);
	FVector AimPoint = Target->GetActorLocation();
	AimPoint.Z = FMath::Clamp(Eye.Z, BoundsOrigin.Z - 0.5f * BoundsExtent.Z, BoundsOrigin.Z + 0.5f * BoundsExtent.Z);

	const FVector Direction = (AimPoint - Eye).GetSafeNormal();
	if (Direction.IsZero())
	{
		return false;
	}

	const FVector Start = Eye - Direction * OriginPullback;
	const FVector End = Eye + Direction * InReach;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RSPlayerMeleeAimed), /*bTraceComplex=*/ true, Avatar);
	TArray<AActor*> Attached;
	Avatar->GetAttachedActors(Attached, /*bResetArray=*/ true, /*bRecursivelyIncludeAttachedActors=*/ true);
	Params.AddIgnoredActors(Attached);
	Params.bReturnPhysicalMaterial = true;

	// Another enemy stepping into the way takes the blow instead; a wall or a teammate leaves it to the fan.
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	const bool bLanded = bHit && IsStrikeTarget(Hit.GetActor());

#if ENABLE_DRAW_DEBUG
	if (RSConsoleVariables::DrawMeleeRaysDuration > 0.0f)
	{
		DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, bLanded ? FColor::Green : FColor::Orange, false,
			RSConsoleVariables::DrawMeleeRaysDuration, 0, 0.5f);
	}
#endif

	if (bLanded)
	{
		OutHit = Hit;
	}
	return bLanded;
}

void URSGameplayAbility_PlayerMelee::Strike()
{
	if (!IsActive())
	{
		return;
	}

	FGameplayAbilityTargetDataHandle TargetData;

	// A lunge's blow goes at the enemy it lunged at: the player committed to it on the press, and the
	// view may have turned since, or be above its head after a lunge from the air. Failing that, the fan
	// at its usual reach first. The fan is sized at its far end, so cast as far as a lunge reaches it
	// opens a third as wide near the player — where a lunge leaves the enemy — and misses one that a
	// normal strike would hit. The far cast is for a lunge the speed cap kept short.
	FHitResult Hit;
	if (FindStrikeOn(StrikeTarget.Get(), StrikeReach, Hit) || FindStrikeTarget(Reach, Hit) || (StrikeReach > Reach && FindStrikeTarget(StrikeReach, Hit)))
	{
		FGameplayAbilityTargetData_SingleTargetHit* SingleHit = new FGameplayAbilityTargetData_SingleTargetHit();
		SingleHit->HitResult = Hit;
		TargetData.Add(SingleHit);
	}

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] strike: %s"), *GetNameSafe(GetAvatarActorFromActorInfo()),
		TargetData.Num() > 0 ? *FString::Printf(TEXT("[%s] at %s"), *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToCompactString()) : TEXT("nothing"));

	// A miss tells the server nothing: there is nothing for it to apply.
	if (TargetData.Num() > 0)
	{
		OnTargetDataReady(TargetData, FGameplayTag());
	}
}

bool URSGameplayAbility_PlayerMelee::ValidateStrike(const FHitResult& Hit) const
{
	AActor* Target = Hit.GetActor();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	FVector Eye;
	FRotator ViewRotation;
	if (!IsStrikeTarget(Target) || !Avatar || !World || !GetStrikeView(Eye, ViewRotation))
	{
		return false;
	}

	// Within reach of the server's own view point, at the lunge's reach if the server lunged too.
	const float MaxDistance = GetAllowedDistance(StrikeReach);
	const float Distance = FVector::Dist(Eye, Hit.ImpactPoint);
	if (Distance > MaxDistance)
	{
		UE_LOG(LogRSAbility, Verbose, TEXT("Strike on [%s] refused: %.0f cm from the server's eye, %.0f allowed."), *GetNameSafe(Target), Distance, MaxDistance);
		return false;
	}

	// On the enemy it names, or near enough for where it has moved to since.
	FVector BoundsOrigin;
	FVector BoundsExtent;
	Target->GetActorBounds(/*bOnlyCollidingComponents=*/ true, BoundsOrigin, BoundsExtent);
	const float OffBounds = FMath::Sqrt(FBox(BoundsOrigin - BoundsExtent, BoundsOrigin + BoundsExtent).ComputeSquaredDistanceToPoint(Hit.ImpactPoint));
	if (OffBounds > ServerValidationTolerance)
	{
		UE_LOG(LogRSAbility, Verbose, TEXT("Strike on [%s] refused: the point is %.0f cm off it."), *GetNameSafe(Target), OffBounds);
		return false;
	}

	// And no wall between: only the level's geometry counts here, since the enemies in front of it
	// have moved on the server since the client looked.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RSPlayerMeleeValidate), /*bTraceComplex=*/ true, Avatar);
	Params.AddIgnoredActor(Target);
	FCollisionObjectQueryParams WorldOnly;
	WorldOnly.AddObjectTypesToQuery(ECC_WorldStatic);
	WorldOnly.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Blocker;
	if (World->LineTraceSingleByObjectType(Blocker, Eye, Hit.ImpactPoint, WorldOnly, Params))
	{
		UE_LOG(LogRSAbility, Verbose, TEXT("Strike on [%s] refused: [%s] is in the way."), *GetNameSafe(Target), *GetNameSafe(Blocker.GetActor()));
		return false;
	}

	return true;
}

void URSGameplayAbility_PlayerMelee::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !ASC->FindAbilitySpecFromHandle(CurrentSpecHandle))
	{
		return;
	}

	FScopedPredictionWindow ScopedPrediction(ASC);

	// Taken by value: anything called below could otherwise invalidate the data mid-use.
	const FGameplayAbilityTargetDataHandle TargetData(MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));
	const FGameplayAbilityTargetData* Single = TargetData.Num() > 0 ? TargetData.Get(0) : nullptr;
	const bool bServer = CurrentActorInfo->IsNetAuthority();

	if (CurrentActorInfo->IsLocallyControlled() && !bServer)
	{
		ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
			TargetData, ApplicationTag, ASC->ScopedPredictionKey);
	}

	// The plan: the strike on screen, for its length, on every machine that runs it. Here, inside the
	// plan's prediction window on both the striker and the server, so the striker plays it now and the
	// server's copy reaches everybody else.
	if (Single && Single->GetScriptStruct() == FRSGameplayAbilityTargetData_MeleePlan::StaticStruct())
	{
		const FRSGameplayAbilityTargetData_MeleePlan* Plan = static_cast<const FRSGameplayAbilityTargetData_MeleePlan*>(Single);
		int32 Variant = Plan->Variant;

#if WITH_SERVER_CODE
		// A remote striker's plan, run here once the server agrees with it. A listen server's own player
		// began its strike as it decided it.
		if (bServer && !CurrentActorInfo->IsLocallyControlled())
		{
			const URSMeleeStrikeSet* StrikeSet = GetStrikeSet();
			const int32 NumStrikes = StrikeSet ? StrikeSet->GetNumStrikes() : 0;
			AActor* LungeTarget = Plan->LungeTarget.Get();
			const bool bLunge = Variant == INDEX_NONE && ValidateLungeTarget(LungeTarget);

			// A strike that is not the set's is the first; a refused lunge is still played, but not run.
			if (Variant != INDEX_NONE && (Variant < 0 || Variant >= FMath::Max(NumStrikes, 1)))
			{
				Variant = 0;
			}
			BeginStrike(Variant, bLunge ? LungeTarget : nullptr);
		}
#endif

		FGameplayCueParameters CueParameters;
		CueParameters.RawMagnitude = static_cast<float>(Variant);
		K2_AddGameplayCueWithParams(RSGameplayTags::Cue_Weapon_Melee, CueParameters, /*bRemoveOnAbilityEnd=*/ true);

		if (bServer)
		{
			ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}
		return;
	}

	// The blow.
	const FHitResult* Hit = Single ? Single->GetHitResult() : nullptr;
	if (!Hit)
	{
		return;
	}

	FGameplayCueParameters ImpactParameters;
	ImpactParameters.Location = Hit->ImpactPoint;
	ImpactParameters.Normal = Hit->ImpactNormal;

	// The striker sees its own blow land at once; the server's verdict decides the damage and what
	// everybody else sees.
	if (!bServer)
	{
		K2_ExecuteGameplayCueWithParams(RSGameplayTags::Cue_Weapon_MeleeImpact, ImpactParameters);
	}

#if WITH_SERVER_CODE
	if (bServer)
	{
		if (ValidateStrike(*Hit) && DamageEffect)
		{
			FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
			if (SpecHandle.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Damage, Damage);

				// Through the target data, so the hit result reaches the damage execution's context.
				ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetData);

				UE_LOG(LogRSAbility, Verbose, TEXT("[%s] strike landed on [%s] for %.0f"),
					*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(Hit->GetActor()), Damage);
			}

			K2_ExecuteGameplayCueWithParams(RSGameplayTags::Cue_Weapon_MeleeImpact, ImpactParameters);
		}
		else if (!DamageEffect)
		{
			UE_LOG(LogRSAbility, Error, TEXT("[%s] has no DamageEffect; the strike landed but deals nothing."), *GetNameSafe(this));
		}

		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
#endif
}

void URSGameplayAbility_PlayerMelee::FinishStrike()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}
