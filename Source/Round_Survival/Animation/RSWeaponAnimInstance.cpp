// Round Survival

#include "Animation/RSWeaponAnimInstance.h"

#include "Core/RSPlayerCharacter.h"

void URSWeaponAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	const AActor* WeaponActor = GetOwningActor();
	Holder = WeaponActor ? Cast<ARSPlayerCharacter>(WeaponActor->GetOwner()) : nullptr;
}

void URSWeaponAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ARSPlayerCharacter* HoldingPlayer = Holder.Get();
	AimDownSightsAlpha = HoldingPlayer ? HoldingPlayer->GetAimDownSightsAlpha() : 0.0f;
}
