// Round Survival

#include "Enemies/RSEnemyDefinition.h"

URSEnemyDefinition::URSEnemyDefinition()
{
	// One entry per tier, measured against the animation that plays there rather than chosen by
	// feel. See TierSpeeds on the header.
	TierSpeeds =
	{
		{ ERSMoveTier::Shamble, 45.0f },
		{ ERSMoveTier::Walk, 90.0f },
		{ ERSMoveTier::Run, 190.0f },
		{ ERSMoveTier::Sprint, 340.0f },
	};
}

float URSEnemyDefinition::GetScrapReward(const FGameplayTag& RewardTag) const
{
	const float* Amount = ScrapRewards.Find(RewardTag);
	return Amount ? *Amount : 0.0f;
}
