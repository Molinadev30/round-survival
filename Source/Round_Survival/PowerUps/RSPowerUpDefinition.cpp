// Round Survival

#include "PowerUps/RSPowerUpDefinition.h"

#include "PowerUps/RSPowerUpEffect.h"

void URSPowerUpDefinition::Activate(const FRSPowerUpContext& Context) const
{
	for (const URSPowerUpEffect* Effect : Effects)
	{
		if (Effect)
		{
			Effect->Apply(Context);
		}
	}
}
