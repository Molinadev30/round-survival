// Round Survival

#pragma once

#include "MVVMViewModelBase.h"

#include "RSPowerUpsViewModel.generated.h"

/**
 * One timed power-up the team has right now, as the HUD shows it: its colour and when it runs out.
 *
 * The end is on this machine's world clock, the one the ability system keeps an active effect's start
 * time on: a client's copy of a replicated effect is moved onto it as it arrives, and again when
 * picking the power-up up a second time restarts it.
 */
USTRUCT(BlueprintType)
struct FRSActivePowerUp
{
	GENERATED_BODY()

	/** The power-up's colour, the one its pickup is tinted with. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Power-Up")
	FLinearColor Color = FLinearColor::White;

	/** World time it runs out at, on this machine. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Power-Up")
	double EndWorldTime = 0.0;

	bool operator==(const FRSActivePowerUp& Other) const
	{
		return Color == Other.Color && EndWorldTime == Other.EndWorldTime;
	}
};

/**
 * The timed power-ups the team has right now, in the order they were picked up: Scrap Surge, Lethal
 * Rounds, Markdown. Read from the local player's own ability system, which every pickup reaches, down
 * or dead included, so a spectator still sees the team's. Fed by URSHUDSubsystem; empty once the
 * match is lost.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSPowerUpsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetActivePowerUps(const TArray<FRSActivePowerUp>& InActivePowerUps);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Power-Up", meta = (AllowPrivateAccess = true))
	TArray<FRSActivePowerUp> ActivePowerUps;
};
