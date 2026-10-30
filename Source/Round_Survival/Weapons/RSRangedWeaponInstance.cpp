// Round Survival

#include "Weapons/RSRangedWeaponInstance.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/RSHandlingSet.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Physics/RSPhysicalMaterialWithTags.h"
#include "RSGameplayTags.h"

URSRangedWeaponInstance::URSRangedWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Usable defaults so a weapon subclass that never touches the curves still behaves sanely:
	// one point of heat per shot, two points a second back off.
	HeatToHeatPerShotCurve.EditorCurveData.AddKey(0.0f, 1.0f);
	HeatToCoolDownPerSecondCurve.EditorCurveData.AddKey(0.0f, 2.0f);
}

void URSRangedWeaponInstance::OnEquipped()
{
	Super::OnEquipped();

	// Drawing a weapon should not hand out a free perfectly accurate shot, so heat starts in the
	// middle of its range rather than at zero.
	float MinHeat = 0.0f;
	float MaxHeat = 0.0f;
	ComputeHeatRange(MinHeat, MaxHeat);
	CurrentHeat = (MinHeat + MaxHeat) * 0.5f;

	CurrentSpreadAngle = HeatToSpreadCurve.GetRichCurveConst()->Eval(CurrentHeat);

	CurrentSpreadAngleMultiplier = 1.0f;
	MovingSpreadBonus = 0.0f;
	AimingMultiplier = 1.0f;
	StandingStillMultiplier = 1.0f;
	CrouchingMultiplier = 1.0f;
	JumpFallMultiplier = 1.0f;
}

void URSRangedWeaponInstance::Tick(float DeltaSeconds)
{
	if (!GetPawn())
	{
		return;
	}

	const bool bMinSpread = UpdateSpread(DeltaSeconds);
	const bool bMinMultipliers = UpdateMultipliers(DeltaSeconds);

	bHasFirstShotAccuracy = bAllowFirstShotAccuracy && bMinSpread && bMinMultipliers;
}

void URSRangedWeaponInstance::AddSpread()
{
	const float HeatPerShot = HeatToHeatPerShotCurve.GetRichCurveConst()->Eval(CurrentHeat);
	CurrentHeat = ClampHeat(CurrentHeat + HeatPerShot);

	CurrentSpreadAngle = HeatToSpreadCurve.GetRichCurveConst()->Eval(CurrentHeat);
}

float URSRangedWeaponInstance::GetTimeBetweenShots() const
{
	const float BaseInterval = 60.0f / FMath::Max(RoundsPerMinute, 1.0f);

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
	if (ASC == nullptr)
	{
		return BaseInterval;
	}

	bool bFound = false;
	const float Multiplier = ASC->GetGameplayAttributeValue(URSHandlingSet::GetFireRateMultiplierAttribute(), bFound);

	// The set floors it above zero; a holder without the set fires at the weapon's own rate.
	return (bFound && Multiplier > 0.0f) ? BaseInterval / Multiplier : BaseInterval;
}

float URSRangedWeaponInstance::GetServerShotBudget(double WorldTime) const
{
	// A weapon that has not fired yet starts with a full bucket.
	if (ServerShotBudgetTime < 0.0)
	{
		return ServerShotBurstTolerance;
	}

	const float Refilled = static_cast<float>((WorldTime - ServerShotBudgetTime) / GetTimeBetweenShots());
	return FMath::Min(ServerShotBurstTolerance, ServerShotBudget + FMath::Max(Refilled, 0.0f));
}

bool URSRangedWeaponInstance::HasServerShotBudget(double WorldTime) const
{
	// A hair under one: the shooter keeps the cadence exactly on average, but each shot leaves on
	// a frame boundary, so one honest gap can come in a fraction of a shot short of a whole one.
	constexpr float FrameTolerance = 0.05f;
	return GetServerShotBudget(WorldTime) >= 1.0f - FrameTolerance;
}

void URSRangedWeaponInstance::ConsumeServerShot(double WorldTime)
{
	ServerShotBudget = FMath::Max(GetServerShotBudget(WorldTime) - 1.0f, 0.0f);
	ServerShotBudgetTime = WorldTime;
}

float URSRangedWeaponInstance::GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags) const
{
	const FRichCurve* Curve = DistanceDamageFalloff.GetRichCurveConst();
	return Curve->HasAnyData() ? Curve->Eval(Distance) : 1.0f;
}

float URSRangedWeaponInstance::GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	float CombinedMultiplier = 1.0f;

	if (const URSPhysicalMaterialWithTags* TaggedMaterial = Cast<const URSPhysicalMaterialWithTags>(PhysicalMaterial))
	{
		// Multiplicative, so a material carrying two zone tags cannot silently pick one.
		for (const FGameplayTag& MaterialTag : TaggedMaterial->Tags)
		{
			if (const float* ZoneMultiplier = HitZoneDamageMultiplier.Find(MaterialTag))
			{
				CombinedMultiplier *= *ZoneMultiplier;
			}
		}
	}

	return CombinedMultiplier;
}

void URSRangedWeaponInstance::ComputeHeatRange(float& MinHeat, float& MaxHeat) const
{
	// The heat range is whatever the three curves between them cover: a weapon defines its range
	// by where it put its keys, not by a separate pair of numbers that could disagree with them.
	float MinPerShot = 0.0f;
	float MaxPerShot = 0.0f;
	HeatToHeatPerShotCurve.GetRichCurveConst()->GetTimeRange(MinPerShot, MaxPerShot);

	float MinCoolDown = 0.0f;
	float MaxCoolDown = 0.0f;
	HeatToCoolDownPerSecondCurve.GetRichCurveConst()->GetTimeRange(MinCoolDown, MaxCoolDown);

	float MinSpread = 0.0f;
	float MaxSpread = 0.0f;
	HeatToSpreadCurve.GetRichCurveConst()->GetTimeRange(MinSpread, MaxSpread);

	MinHeat = FMath::Min(FMath::Min(MinPerShot, MinCoolDown), MinSpread);
	MaxHeat = FMath::Max(FMath::Max(MaxPerShot, MaxCoolDown), MaxSpread);
}

void URSRangedWeaponInstance::ComputeSpreadRange(float& MinSpread, float& MaxSpread) const
{
	HeatToSpreadCurve.GetRichCurveConst()->GetValueRange(MinSpread, MaxSpread);
}

bool URSRangedWeaponInstance::UpdateSpread(float DeltaSeconds)
{
	if (GetTimeSinceLastInteractedWith() > SpreadRecoveryCooldownDelay)
	{
		const float CoolDownRate = HeatToCoolDownPerSecondCurve.GetRichCurveConst()->Eval(CurrentHeat);
		CurrentHeat = ClampHeat(CurrentHeat - (CoolDownRate * DeltaSeconds));
		CurrentSpreadAngle = HeatToSpreadCurve.GetRichCurveConst()->Eval(CurrentHeat);
	}

	float MinSpread = 0.0f;
	float MaxSpread = 0.0f;
	ComputeSpreadRange(MinSpread, MaxSpread);

	return FMath::IsNearlyEqual(CurrentSpreadAngle, MinSpread, KINDA_SMALL_NUMBER);
}

bool URSRangedWeaponInstance::UpdateMultipliers(float DeltaSeconds)
{
	constexpr float NearlyEqualThreshold = 0.05f;

	APawn* Pawn = GetPawn();
	if (!Pawn)
	{
		return false;
	}

	const UCharacterMovementComponent* Movement = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());

	// Standing still fades out over a speed band instead of flipping at a threshold, so walking
	// off a bonus is gradual rather than a step in the reticle.
	const float PawnSpeed = Pawn->GetVelocity().Size();
	const FVector2D MovingSpeedBand(StandingStillSpeedThreshold, StandingStillSpeedThreshold + StandingStillToMovingSpeedRange);
	const float StandingStillTarget = FMath::GetMappedRangeValueClamped(
		MovingSpeedBand, FVector2D(SpreadAngleMultiplier_StandingStill, 1.0f), PawnSpeed);
	StandingStillMultiplier = FMath::FInterpTo(StandingStillMultiplier, StandingStillTarget, DeltaSeconds, TransitionRate_StandingStill);
	const bool bStandingStillAtTarget = FMath::IsNearlyEqual(StandingStillMultiplier, SpreadAngleMultiplier_StandingStill,
		FMath::Max(SpreadAngleMultiplier_StandingStill * 0.1f, KINDA_SMALL_NUMBER));

	// The moving bonus fades in over the same band, and is flat above it.
	const float MovingTarget = FMath::GetMappedRangeValueClamped(MovingSpeedBand, FVector2D(0.0f, SpreadAngleBonus_Moving), PawnSpeed);
	MovingSpreadBonus = FMath::FInterpTo(MovingSpreadBonus, MovingTarget, DeltaSeconds, TransitionRate_Moving);
	const bool bNotMoving = MovingSpreadBonus <= NearlyEqualThreshold;

	const bool bIsCrouching = Movement && Movement->IsCrouching();
	const float CrouchingTarget = bIsCrouching ? SpreadAngleMultiplier_Crouching : 1.0f;
	CrouchingMultiplier = FMath::FInterpTo(CrouchingMultiplier, CrouchingTarget, DeltaSeconds, TransitionRate_Crouching);
	const bool bCrouchingAtTarget = FMath::IsNearlyEqual(CrouchingMultiplier, CrouchingTarget, NearlyEqualThreshold);

	const bool bIsFalling = Movement && Movement->IsFalling();
	const float JumpFallTarget = bIsFalling ? SpreadAngleMultiplier_JumpingOrFalling : 1.0f;
	JumpFallMultiplier = FMath::FInterpTo(JumpFallMultiplier, JumpFallTarget, DeltaSeconds, TransitionRate_JumpingOrFalling);
	const bool bJumpFallAtOne = FMath::IsNearlyEqual(JumpFallMultiplier, 1.0f, NearlyEqualThreshold);

	// Aiming is read from the state tag rather than from a camera blend: this project has no
	// camera mode stack, the ADS ability owns that tag, and the tag is what the animation reads
	// too — one source of truth for "is this player aiming".
	bool bIsAiming = false;
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		bIsAiming = ASC->HasMatchingGameplayTag(RSGameplayTags::State_ADS);
	}

	const float AimingTarget = bIsAiming ? SpreadAngleMultiplier_Aiming : 1.0f;
	AimingMultiplier = FMath::FInterpTo(AimingMultiplier, AimingTarget, DeltaSeconds, TransitionRate_Aiming);
	const bool bAimingAtTarget = FMath::IsNearlyEqual(AimingMultiplier, AimingTarget, NearlyEqualThreshold);

	CurrentSpreadAngleMultiplier = AimingMultiplier * StandingStillMultiplier * CrouchingMultiplier * JumpFallMultiplier;

	return bStandingStillAtTarget && bNotMoving && bCrouchingAtTarget && bJumpFallAtOne && bAimingAtTarget;
}
