// Round Survival

#include "Animation/RSViewmodelAnimInstance.h"

#include "GameFramework/Pawn.h"
#include "Weapons/RSFirstPersonRecoilComponent.h"

URSViewmodelAnimInstance::URSViewmodelAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URSViewmodelAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (const APawn* Pawn = TryGetPawnOwner())
	{
		Recoil = Pawn->FindComponentByClass<URSFirstPersonRecoilComponent>();
	}
}

void URSViewmodelAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Game thread: the component writes these on the game thread too. It ticks before the
	// viewmodel does, so this is the current frame's kick, not the last one.
	if (const URSFirstPersonRecoilComponent* RecoilComponent = Recoil.Get())
	{
		RecoilLocation = RecoilComponent->GetLocationOffset();
		RecoilRotation = RecoilComponent->GetRotationOffset();
	}

	// Scaled rather than switched, so the sight slides onto the eye over the same aim-in the arms
	// and the camera take.
	AimDownSightsLocation = AimDownSightsOffset.GetLocation() * AimDownSightsAlpha;
	AimDownSightsRotation = FQuat::Slerp(FQuat::Identity, AimDownSightsOffset.GetRotation(), AimDownSightsAlpha).Rotator();
}
