// Round Survival

#include "Weapons/RSImpactEffects.h"

#include "Physics/RSPhysicalMaterialWithTags.h"

const FRSImpactEffect& URSImpactEffects::FindEffect(const UPhysicalMaterial* PhysicalMaterial) const
{
	if (const URSPhysicalMaterialWithTags* Tagged = Cast<URSPhysicalMaterialWithTags>(PhysicalMaterial))
	{
		for (const TPair<FGameplayTag, FRSImpactEffect>& Zone : HitZones)
		{
			if (Tagged->Tags.HasTagExact(Zone.Key))
			{
				return Zone.Value;
			}
		}
	}

	if (PhysicalMaterial)
	{
		if (const FRSImpactEffect* Surface = Surfaces.Find(PhysicalMaterial->SurfaceType))
		{
			return *Surface;
		}
	}

	return Default;
}
