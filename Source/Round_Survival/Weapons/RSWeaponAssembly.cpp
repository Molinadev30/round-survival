// Round Survival

#include "Weapons/RSWeaponAssembly.h"

ERSLeftHandGrip URSWeaponAssembly::GetLeftHandGrip() const
{
	// Last declared wins, as for the muzzle.
	for (int32 Index = Parts.Num() - 1; Index >= 0; --Index)
	{
		if (Parts[Index].LeftHandGrip != ERSLeftHandGrip::Handguard)
		{
			return Parts[Index].LeftHandGrip;
		}
	}

	return ERSLeftHandGrip::Handguard;
}
