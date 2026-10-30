// Round Survival

#include "Weapons/RSMeleeStrikeSet.h"

#include "Animation/AnimMontage.h"

const FRSMeleeStrike* URSMeleeStrikeSet::GetStrike(int32 Variant) const
{
	if (Variant == INDEX_NONE)
	{
		return &LungeStrike;
	}

	return Strikes.IsValidIndex(Variant) ? &Strikes[Variant] : nullptr;
}

void URSMeleeStrikeSet::GetAllMontages(TArray<UAnimMontage*>& OutFirstPerson, TArray<UAnimMontage*>& OutThirdPerson) const
{
	const auto Add = [&OutFirstPerson, &OutThirdPerson](const FRSMeleeStrike& Strike)
	{
		if (Strike.FirstPersonMontage)
		{
			OutFirstPerson.AddUnique(Strike.FirstPersonMontage);
		}
		if (Strike.ThirdPersonMontage)
		{
			OutThirdPerson.AddUnique(Strike.ThirdPersonMontage);
		}
	};

	for (const FRSMeleeStrike& Strike : Strikes)
	{
		Add(Strike);
	}
	Add(LungeStrike);
}
