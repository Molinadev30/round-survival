// Round Survival

#pragma once

#include "Engine/DataAsset.h"

#include "RSMeleeStrikeSet.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

/**
 * One melee strike as it is animated: the montage pair and the moments in it that the strike's
 * rhythm is built on. Times are as authored, at a play rate of one.
 */
USTRUCT(BlueprintType)
struct FRSMeleeStrike
{
	GENERATED_BODY()

	/** Played on the viewmodel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee")
	TObjectPtr<UAnimMontage> FirstPersonMontage;

	/** Played on the body; what everybody else sees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee")
	TObjectPtr<UAnimMontage> ThirdPersonMontage;

	/** When the weapon reaches what it is swung at: the blow lands here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee", meta = (ForceUnits = s, ClampMin = 0.0))
	float ImpactTime = 0.25f;

	/** When the hands are free again. The rest of the montage is the settle, which anything may cut. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee", meta = (ForceUnits = s, ClampMin = 0.0))
	float Duration = 1.0f;

	/** When the swing is heard through the air. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee", meta = (ForceUnits = s, ClampMin = 0.0))
	float WhooshTime = 0.2f;
};

/**
 * How a weapon is swung: the strikes it alternates between, the one it lunges with, how fast they
 * are played, and what a strike sounds and looks like.
 *
 * Named by the weapon instance, so a weapon held another way brings strikes animated for its own
 * hold. The melee ability takes its timing from here — the moments above, divided by PlayRate — so
 * the blow lands on the frame the animation shows it landing, whichever animation it is.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSMeleeStrikeSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The strike for a variant: 0 and up for the alternating strikes, INDEX_NONE for the lunge. Null when there is none. */
	const FRSMeleeStrike* GetStrike(int32 Variant) const;

	/** How many strikes the set alternates between. */
	int32 GetNumStrikes() const { return Strikes.Num(); }

	/** Every montage in the set, to stop whichever of them is playing. */
	void GetAllMontages(TArray<UAnimMontage*>& OutFirstPerson, TArray<UAnimMontage*>& OutThirdPerson) const;

	/** Played in turn, one per strike. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee")
	TArray<FRSMeleeStrike> Strikes;

	/** Played when the strike lunges, whichever strike was next. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee")
	FRSMeleeStrike LungeStrike;

	/** The speed every strike is played at, and so the speed of its rhythm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee", meta = (ClampMin = 0.01))
	float PlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee")
	TObjectPtr<USoundBase> WhooshSound;

	/** Played where a strike lands on an enemy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee")
	TObjectPtr<USoundBase> ImpactSound;

	/** Spawned where a strike lands on an enemy, facing back along the blow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Melee")
	TObjectPtr<UNiagaraSystem> ImpactEffect;
};
