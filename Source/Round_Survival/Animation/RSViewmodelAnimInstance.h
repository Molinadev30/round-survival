// Round Survival

#pragma once

#include "Animation/RSAnimInstance.h"

#include "RSViewmodelAnimInstance.generated.h"

class URSFirstPersonRecoilComponent;

/**
 * Animation instance for the first-person viewmodel: everything the body has, plus the
 * procedural recoil and the sight offset.
 *
 * The kick is computed by URSFirstPersonRecoilComponent on the pawn and only read here. The
 * viewmodel's graph applies it to the bone the weapon is held by and then brings both hands back
 * onto the weapon with IK, so the weapon and hands kick while the shoulders stay put.
 *
 * The sight offset goes on the same bone, the same way. The aimed animations bring the weapon up to
 * the eye, but they are authored for one sight and every assembly carries its own; the drawn weapon
 * hands this its assembly's AimDownSightsOffset, and it is faded in with AimDownSightsAlpha.
 */
UCLASS()
class ROUND_SURVIVAL_API URSViewmodelAnimInstance : public URSAnimInstance
{
	GENERATED_BODY()

public:
	URSViewmodelAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * The drawn weapon's sight offset, at full aim. Kept when the weapon is put away: the aim has
	 * already faded it out, and the next weapon brings its own.
	 */
	void SetAimDownSightsOffset(const FTransform& Offset) { AimDownSightsOffset = Offset; }

protected:
	//~UAnimInstance interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	//~End of UAnimInstance interface

	/** The recoil kick on the weapon bone, in cm along the mesh's own axes. Added, not set. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Recoil")
	FVector RecoilLocation = FVector::ZeroVector;

	/** The recoil kick on the weapon bone, in degrees about the mesh's own axes. Added, not set. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Recoil")
	FRotator RecoilRotation = FRotator::ZeroRotator;

	/** The sight offset on the weapon bone, in cm along the mesh's own axes, scaled by the aim. Added, not set. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	FVector AimDownSightsLocation = FVector::ZeroVector;

	/** The same, in degrees about the mesh's own axes. Added, not set. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	FRotator AimDownSightsRotation = FRotator::ZeroRotator;

private:
	TWeakObjectPtr<const URSFirstPersonRecoilComponent> Recoil;

	FTransform AimDownSightsOffset = FTransform::Identity;
};
