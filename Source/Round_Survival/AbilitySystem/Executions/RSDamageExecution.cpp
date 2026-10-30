// Round Survival

#include "AbilitySystem/Executions/RSDamageExecution.h"

#include "AbilitySystem/Attributes/RSCombatSet.h"
#include "AbilitySystem/Attributes/RSHealthSet.h"
#include "AbilitySystem/RSAbilitySourceInterface.h"
#include "AbilitySystemComponent.h"
#include "Core/RSPlayerState.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

namespace
{
	struct FRSDamageStatics
	{
		FGameplayEffectAttributeCaptureDefinition OutgoingDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition IncomingDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition TargetHealthDef;

		FRSDamageStatics()
		{
			// Snapshotted: a buff that runs out while the bullet is in the air should still count,
			// and one picked up after the trigger was pulled should not.
			OutgoingDamageMultiplierDef = FGameplayEffectAttributeCaptureDefinition(
				URSCombatSet::GetOutgoingDamageMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Source, /*bSnapshot=*/ true);

			// Not snapshotted: the target's armour is whatever it is when the hit lands.
			IncomingDamageMultiplierDef = FGameplayEffectAttributeCaptureDefinition(
				URSCombatSet::GetIncomingDamageMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot=*/ false);

			// Only read under Lethal Rounds, which deals whatever the target has left.
			TargetHealthDef = FGameplayEffectAttributeCaptureDefinition(
				URSHealthSet::GetHealthAttribute(), EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot=*/ false);
		}
	};

	const FRSDamageStatics& DamageStatics()
	{
		static FRSDamageStatics Statics;
		return Statics;
	}

	/**
	 * Whether this exchange may deal damage at all.
	 *
	 * Players are on one side and everything else on the other. Both player ASCs live on a player
	 * state, which is the cheapest reliable way to tell them apart until there is a reason for teams.
	 */
	bool IsDamageAllowed(const UAbilitySystemComponent* SourceASC, const UAbilitySystemComponent* TargetASC)
	{
		if (!SourceASC || !TargetASC)
		{
			return false;
		}

		const bool bSourceIsPlayer = Cast<ARSPlayerState>(SourceASC->GetOwnerActor()) != nullptr;
		const bool bTargetIsPlayer = Cast<ARSPlayerState>(TargetASC->GetOwnerActor()) != nullptr;

		return !(bSourceIsPlayer && bTargetIsPlayer);
	}
}

URSDamageExecution::URSDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().OutgoingDamageMultiplierDef);
	RelevantAttributesToCapture.Add(DamageStatics().IncomingDamageMultiplierDef);
	RelevantAttributesToCapture.Add(DamageStatics().TargetHealthDef);
}

void URSDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FGameplayEffectContextHandle& Context = Spec.GetContext();

	if (!IsDamageAllowed(ExecutionParams.GetSourceAbilitySystemComponent(), ExecutionParams.GetTargetAbilitySystemComponent()))
	{
		return;
	}

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	const float BaseDamage = FMath::Max(0.0f,
		Spec.GetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Damage, /*WarnIfNotFound=*/ true, /*DefaultIfNotFound=*/ 0.0f));

	float OutgoingMultiplier = 1.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().OutgoingDamageMultiplierDef, EvaluateParameters, OutgoingMultiplier);

	float IncomingMultiplier = 1.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().IncomingDamageMultiplierDef, EvaluateParameters, IncomingMultiplier);

	// Attenuation is asked of whatever fired the shot. A source that is not an ability source —
	// a trap, a debug command — deals its base damage unattenuated.
	float DistanceAttenuation = 1.0f;
	float ZoneMultiplier = 1.0f;
	float Distance = 0.0f;

	const IRSAbilitySourceInterface* AbilitySource = Cast<IRSAbilitySourceInterface>(Context.GetSourceObject());
	const FHitResult* HitResult = Context.GetHitResult();

	if (AbilitySource && HitResult)
	{
		// The origin is the server's own muzzle when the shooter set one. The hit's trace start is
		// only a fallback: on a client-reported hit it is the client's word for where the shot began,
		// and trusting it would let a cheat claim point-blank damage from across the map.
		const FVector Origin = Context.HasOrigin() ? Context.GetOrigin() : HitResult->TraceStart;
		Distance = FVector::Dist(Origin, HitResult->ImpactPoint);

		DistanceAttenuation = FMath::Max(0.0f, AbilitySource->GetDistanceAttenuation(Distance, EvaluateParameters.SourceTags, EvaluateParameters.TargetTags));
		ZoneMultiplier = FMath::Max(0.0f, AbilitySource->GetPhysicalMaterialAttenuation(HitResult->PhysMaterial.Get(), EvaluateParameters.SourceTags, EvaluateParameters.TargetTags));
	}

	// The combat set already clamps both multipliers at zero; clamping again here costs nothing and
	// keeps a negative value from an unclamped future source from turning damage into healing.
	float DamageDone = BaseDamage * DistanceAttenuation * ZoneMultiplier
		* FMath::Max(0.0f, OutgoingMultiplier) * FMath::Max(0.0f, IncomingMultiplier);

	// Lethal Rounds: any hit that lands kills. Read from the source's tags as the spec captured them,
	// like the multiplier, so a shot fired in the last moment of the power-up still counts. Only a
	// player carries the tag, and players cannot damage each other, so the target is an enemy.
	if (BaseDamage > 0.0f && EvaluateParameters.SourceTags && EvaluateParameters.SourceTags->HasTag(RSGameplayTags::PowerUp_LethalRounds))
	{
		float TargetHealth = 0.0f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().TargetHealthDef, EvaluateParameters, TargetHealth);
		DamageDone = FMath::Max(DamageDone, TargetHealth);

		UE_LOG(LogRSAbility, Verbose, TEXT("Lethal Rounds: the hit deals the %.1f health left."), TargetHealth);
	}

	UE_LOG(LogRSAbility, Verbose, TEXT("Damage on [%s]: base %.1f x distance %.2f (%.0f cm) x zone %.2f x outgoing %.2f x incoming %.2f = %.1f"),
		*GetNameSafe(ExecutionParams.GetTargetAbilitySystemComponent() ? ExecutionParams.GetTargetAbilitySystemComponent()->GetAvatarActor() : nullptr),
		BaseDamage, DistanceAttenuation, Distance, ZoneMultiplier, OutgoingMultiplier, IncomingMultiplier, DamageDone);

	if (DamageDone > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(URSHealthSet::GetDamageAttribute(), EGameplayModOp::Additive, DamageDone));
	}
#endif
}
