// Round Survival

#include "Rounds/RSRoundSettings.h"

#include <initializer_list>

namespace RSRoundSettings
{
	/** Adds a straight line of keys to a curve, for the defaults a new asset starts with. */
	static void SetCurveKeys(FRuntimeFloatCurve& Curve, std::initializer_list<TPair<float, float>> Keys)
	{
		FRichCurve* RichCurve = Curve.GetRichCurve();
		if (!RichCurve)
		{
			return;
		}

		RichCurve->Reset();
		for (const TPair<float, float>& Key : Keys)
		{
			RichCurve->AddKey(Key.Key, Key.Value);
		}

		// Linear between the authored rounds, and flat past the last one: the compounding rules
		// take over there, and an extrapolated curve would fight them.
		for (auto KeyIterator = RichCurve->GetKeyHandleIterator(); KeyIterator; ++KeyIterator)
		{
			RichCurve->SetKeyInterpMode(*KeyIterator, RCIM_Linear);
		}
	}

	/** Reads a curve, or returns the fallback when the asset has no keys at all. */
	static float EvalCurve(const FRuntimeFloatCurve& Curve, float Time, float Fallback)
	{
		const FRichCurve* RichCurve = Curve.GetRichCurveConst();
		if (!RichCurve || RichCurve->GetNumKeys() == 0)
		{
			return Fallback;
		}

		return RichCurve->Eval(Time);
	}
}

FRSMoveTierMix::FRSMoveTierMix()
{
	// The opening rounds are all shamblers, walkers take over around four, runners appear at six
	// and own the round by twelve, and sprinters only show up late and never take it over on
	// their own. Starting values to play against, not a balance pass.
	RSRoundSettings::SetCurveKeys(ShambleWeight, { { 1.0f, 1.0f }, { 4.0f, 0.5f }, { 7.0f, 0.0f } });
	RSRoundSettings::SetCurveKeys(WalkWeight,    { { 1.0f, 0.0f }, { 3.0f, 0.5f }, { 6.0f, 1.0f }, { 12.0f, 0.4f }, { 20.0f, 0.1f } });
	RSRoundSettings::SetCurveKeys(RunWeight,     { { 5.0f, 0.0f }, { 8.0f, 0.4f }, { 12.0f, 1.0f }, { 20.0f, 0.8f } });
	RSRoundSettings::SetCurveKeys(SprintWeight,  { { 11.0f, 0.0f }, { 15.0f, 0.2f }, { 25.0f, 0.6f } });
}

void FRSMoveTierMix::GetWeights(int32 RoundNumber, float& OutShamble, float& OutWalk, float& OutRun, float& OutSprint) const
{
	const float Round = static_cast<float>(RoundNumber);

	OutShamble = FMath::Max(RSRoundSettings::EvalCurve(ShambleWeight, Round, 1.0f), 0.0f);
	OutWalk    = FMath::Max(RSRoundSettings::EvalCurve(WalkWeight,    Round, 0.0f), 0.0f);
	OutRun     = FMath::Max(RSRoundSettings::EvalCurve(RunWeight,     Round, 0.0f), 0.0f);
	OutSprint  = FMath::Max(RSRoundSettings::EvalCurve(SprintWeight,  Round, 0.0f), 0.0f);
}

ERSMoveTier FRSMoveTierMix::DrawTier(int32 RoundNumber) const
{
	float Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	GetWeights(RoundNumber, Weights[0], Weights[1], Weights[2], Weights[3]);

	const float TotalWeight = Weights[0] + Weights[1] + Weights[2] + Weights[3];
	if (TotalWeight <= 0.0f)
	{
		// An asset with every weight at zero for this round. Slowest tier rather than no enemy.
		return ERSMoveTier::Shamble;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.0f)
		{
			return static_cast<ERSMoveTier>(Index);
		}
	}

	return ERSMoveTier::Sprint;
}

URSRoundSettings::URSRoundSettings()
{
	// A quota that roughly doubles every five rounds early on, then keeps climbing.
	RSRoundSettings::SetCurveKeys(EnemyQuotaCurve,
		{ { 1.0f, 6.0f }, { 5.0f, 18.0f }, { 10.0f, 34.0f }, { 15.0f, 52.0f }, { 20.0f, 72.0f } });

	// Health up to the hand-off round; compounding carries it from there.
	RSRoundSettings::SetCurveKeys(EnemyHealthCurve,
		{ { 1.0f, 150.0f }, { 5.0f, 290.0f }, { 10.0f, 550.0f } });

	// Two seconds apart at first, half a second by the late rounds.
	RSRoundSettings::SetCurveKeys(SpawnIntervalCurve,
		{ { 1.0f, 2.0f }, { 10.0f, 1.0f }, { 20.0f, 0.5f } });

	// Long enough at first to find the layout, short enough later to stay uncomfortable.
	RSRoundSettings::SetCurveKeys(IntermissionCurve,
		{ { 1.0f, 12.0f }, { 5.0f, 9.0f }, { 15.0f, 6.0f } });
}

int32 URSRoundSettings::GetEnemyQuota(int32 RoundNumber, int32 PlayerCount) const
{
	const float BaseQuota = RSRoundSettings::EvalCurve(EnemyQuotaCurve, static_cast<float>(RoundNumber), 6.0f);
	const int32 ExtraPlayers = FMath::Max(PlayerCount - 1, 0);

	return FMath::Max(FMath::RoundToInt(BaseQuota * (1.0f + QuotaPerExtraPlayer * ExtraPlayers)), 1);
}

float URSRoundSettings::GetEnemyHealth(int32 RoundNumber) const
{
	const int32 CurveRound = FMath::Min(RoundNumber, CompoundingStartRound);
	const float BaseHealth = RSRoundSettings::EvalCurve(EnemyHealthCurve, static_cast<float>(CurveRound), 150.0f);

	if (RoundNumber <= CompoundingStartRound)
	{
		return BaseHealth;
	}

	// Past the authored rounds the curve stops and the multiplier takes over, once per round.
	return BaseHealth * FMath::Pow(HealthCompoundingFactor, static_cast<float>(RoundNumber - CompoundingStartRound));
}

float URSRoundSettings::GetSpawnInterval(int32 RoundNumber) const
{
	return FMath::Max(RSRoundSettings::EvalCurve(SpawnIntervalCurve, static_cast<float>(RoundNumber), 1.5f), 0.05f);
}

float URSRoundSettings::GetIntermissionSeconds(int32 RoundNumber) const
{
	return FMath::Max(RSRoundSettings::EvalCurve(IntermissionCurve, static_cast<float>(RoundNumber), 10.0f), 0.0f);
}

int32 URSRoundSettings::GetMaxAlive(int32 PlayerCount) const
{
	const int32 ExtraPlayers = FMath::Max(PlayerCount - 1, 0);
	return FMath::Max(MaxAliveBase + MaxAlivePerExtraPlayer * ExtraPlayers, 1);
}

ERSMoveTier URSRoundSettings::DrawMoveTier(int32 RoundNumber) const
{
	return MoveTierMix.DrawTier(RoundNumber);
}
